#include "initializer.h"

#include <string>

#include <openssl/x509.h>
#include <boost/scoped_array.hpp>

#include "bootstrap/bootstrap.h"
#include "crypto/keymanager.h"
#include "logging/logging.h"

// Postcondition: device_id is in the storage
bool Initializer::initDeviceId() {
  // if device_id is already stored, just return
  std::string device_id;
  if (storage->loadDeviceId(&device_id)) {
    return true;
  }

  // if device_id is specified in config, just use it, otherwise generate a  random one
  device_id = config.device_id;
  if (device_id.empty()) {
    if (config.mode == kAutomatic) {
      device_id = Utils::genPrettyName();
    } else if (config.mode == kImplicit) {
      device_id = keys.getCN();
    } else {
      LOG_ERROR << "Unknown provisioning method";
      return false;
    }
  }

  storage->storeDeviceId(device_id);
  return true;
}
void Initializer::resetDeviceId() { storage->clearDeviceId(); }

// Postcondition [(serial, hw_id)] is in the storage
bool Initializer::initEcuSerials() {
  std::vector<std::pair<std::string, std::string> > ecu_serials;

  // TODO: the assumption now is that the set of connected ECUs doesn't change, but it might obviously
  //   not be the case. ECU discovery seems to be a big story and should be worked on accordingly.
  if (storage->loadEcuSerials(&ecu_serials)) {
    return true;
  }

  std::string primary_ecu_serial_local = config.primary_ecu_serial;
  if (primary_ecu_serial_local.empty()) {
    primary_ecu_serial_local = Crypto::getKeyId(keys.getUptanePublicKey());
  }

  std::string primary_ecu_hardware_id = config.primary_ecu_hardware_id;
  if (primary_ecu_hardware_id.empty()) {
    primary_ecu_hardware_id = Utils::getHostname();
    if (primary_ecu_hardware_id == "") {
      return false;
    }
  }

  ecu_serials.emplace_back(primary_ecu_serial_local, primary_ecu_hardware_id);

  for (auto it = secondary_info.begin(); it != secondary_info.end(); ++it) {
    ecu_serials.emplace_back(it->second->getSerial(), it->second->getHwId());
  }

  storage->storeEcuSerials(ecu_serials);
  return true;
}

void Initializer::resetEcuSerials() { storage->clearEcuSerials(); }

// Postcondition: (public, private) is in the storage. It should not be stored until secondaries are provisioned
bool Initializer::initPrimaryEcuKeys() { return keys.generateUptaneKeyPair().size() != 0u; }

void Initializer::resetEcuKeys() { storage->clearPrimaryKeys(); }

bool Initializer::loadSetTlsCreds() {
  keys.copyCertsToCurl(&http_client);
  return keys.isOk();
}

// Postcondition: TLS credentials are in the storage
InitRetCode Initializer::initTlsCreds() {
  if (loadSetTlsCreds()) {
    return INIT_RET_OK;
  }

  if (config.mode != kAutomatic) {
    LOG_ERROR << "Credentials not found";
    return INIT_RET_STORAGE_FAILURE;
  }

  // Autoprovision is needed and possible => autoprovision

  // set bootstrap credentials
  Bootstrap boot(config.provision_path, config.p12_password);
  http_client.setCerts(boot.getCa(), kFile, boot.getCert(), kFile, boot.getPkey(), kFile);

  Json::Value data;
  std::string device_id;
  if (!storage->loadDeviceId(&device_id)) {
    LOG_ERROR << "device_id unknown during autoprovisioning process";
    return INIT_RET_STORAGE_FAILURE;
  }
  data["deviceId"] = device_id;
  data["ttl"] = config.expiry_days;
  HttpResponse response = http_client.post(config.server + "/devices", data);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() && resp_code.asString() == "device_already_registered") {
      LOG_ERROR << "Device id" << device_id << "is occupied";
      return INIT_RET_OCCUPIED;
    }
    LOG_ERROR << "Autoprovisioning failed, response: " << response.body;
    return INIT_RET_SERVER_FAILURE;
  }

  std::string pkey;
  std::string cert;
  std::string ca;
  StructGuard<BIO> device_p12(BIO_new_mem_buf(response.body.c_str(), response.body.size()), BIO_vfree);
  if (!Crypto::parseP12(device_p12.get(), "", &pkey, &cert, &ca)) {
    LOG_ERROR << "Received a malformed P12 package from the server";
    return INIT_RET_BAD_P12;
  }
  storage->storeTlsCreds(ca, cert, pkey);

  // set provisioned credentials
  if (!loadSetTlsCreds()) {
    LOG_ERROR << "Failed to set provisioned credentials";
    return INIT_RET_STORAGE_FAILURE;
  }

  LOG_INFO << "Provisioned successfully on Device Gateway";
  return INIT_RET_OK;
}

void Initializer::resetTlsCreds() {
  if (config.mode != kImplicit) {
    storage->clearTlsCreds();
  }
}

// Postcondition: "ECUs registered" flag set in the storage
InitRetCode Initializer::initEcuRegister() {
  if (storage->loadEcuRegistered()) {
    return INIT_RET_OK;
  }

  std::string primary_public = keys.getUptanePublicKey();

  if (primary_public.empty()) {
    return INIT_RET_STORAGE_FAILURE;
  }

  std::vector<std::pair<std::string, std::string> > ecu_serials;
  // initEcuSerials should have been called by this point
  if (!storage->loadEcuSerials(&ecu_serials) || ecu_serials.size() < 1) {
    return INIT_RET_STORAGE_FAILURE;
  }

  Json::Value all_ecus;
  all_ecus["primary_ecu_serial"] = ecu_serials[0].first;
  all_ecus["ecus"] = Json::arrayValue;
  {
    Json::Value primary_ecu;
    primary_ecu["hardware_identifier"] = ecu_serials[0].second;
    primary_ecu["ecu_serial"] = ecu_serials[0].first;
    primary_ecu["clientKey"]["keytype"] = keyTypeToString(keys.getUptaneKeyType());
    primary_ecu["clientKey"]["keyval"]["public"] = primary_public;
    all_ecus["ecus"].append(primary_ecu);
  }

  for (auto it = secondary_info.cbegin(); it != secondary_info.cend(); it++) {
    Json::Value ecu;
    auto public_key = it->second->getPublicKey();
    ecu["hardware_identifier"] = it->second->getHwId();
    ecu["ecu_serial"] = it->second->getSerial();
    ecu["clientKey"]["keytype"] = keyTypeToString(public_key.first);
    ecu["clientKey"]["keyval"]["public"] = public_key.second;
    all_ecus["ecus"].append(ecu);
  }

  HttpResponse response = http_client.post(config.ecu_registration_endpoint, all_ecus);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() &&
        (resp_code.asString() == "ecu_already_registered" || resp_code.asString() == "device_already_registered")) {
      LOG_ERROR << "Some ECU is already registered";
      return INIT_RET_OCCUPIED;
    }
    LOG_ERROR << "Error registering device on Uptane, response: " << response.body;
    return INIT_RET_SERVER_FAILURE;
  }
  // do not call storage->storeEcuRegistered(), it will be called from the top-level Init function after the
  // acknowledgement
  LOG_INFO << "ECUs have been successfully registered to the server";
  return INIT_RET_OK;
}

// Postcondition: "ECUs registered" flag set in the storage
Initializer::Initializer(const ProvisionConfig& config, const std::shared_ptr<INvStorage>& storage,
                         HttpInterface& http_client, KeyManager& keys,
                         const std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info)
    : config(config), storage(storage), http_client(http_client), keys(keys), secondary_info(secondary_info) {
  success = false;
  for (int i = 0; i < MaxInitializationAttempts; i++) {
    if (!initDeviceId()) {
      LOG_ERROR << "Device ID generation failed, abort initialization";
      return;
    }

    InitRetCode ret_code = initTlsCreds();
    // if a device with the same ID has already been registered to the server,
    // generate a new one
    if (ret_code == INIT_RET_OCCUPIED) {
      resetDeviceId();
      LOG_INFO << "Device name is already registered, restart";
      continue;
    }
    if (ret_code != INIT_RET_OK) {
      LOG_ERROR << "Autoprovisioning failed, abort initialization";
      return;
    }

    if (!initPrimaryEcuKeys()) {
      LOG_ERROR << "ECU key generation failed, abort initialization";
      return;
    }
    if (!initEcuSerials()) {
      LOG_ERROR << "ECU serial generation failed, abort initialization";
      return;
    }

    ret_code = initEcuRegister();
    // if ECUs with same ID have been registered to the server, we don't have a
    // clear remediation path right now, just ignore the error
    if (ret_code == INIT_RET_OCCUPIED) {
      LOG_INFO << "ECU serial is already registered";
    } else if (ret_code != INIT_RET_OK) {
      LOG_ERROR << "ECU registration failed, abort initialization";
      return;
    }

    // TODO: acknowledge on server _before_ setting the flag
    storage->storeEcuRegistered();
    success = true;
    return;
  }
  LOG_ERROR << "Initialization failed after " << MaxInitializationAttempts << " attempts";
}
