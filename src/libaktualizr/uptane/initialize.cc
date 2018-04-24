#include <string>

#include <openssl/x509.h>
#include <boost/scoped_array.hpp>

#include "bootstrap.h"
#include "logging/logging.h"
#include "uptane/uptanerepository.h"
#include "utilities/keymanager.h"

namespace Uptane {

// Postcondition: device_id is in the storage
bool Repository::initDeviceId(const ProvisionConfig& provision_config, const UptaneConfig& uptane_config) {
  // if device_id is already stored, just return
  std::string device_id;
  if (storage->loadDeviceId(&device_id)) {
    return true;
  }

  // if device_id is specified in config, just use it, otherwise generate a  random one
  device_id = uptane_config.device_id;
  if (device_id.empty()) {
    if (provision_config.mode == kAutomatic) {
      device_id = Utils::genPrettyName();
    } else if (provision_config.mode == kImplicit) {
      device_id = KeyManager(storage, config.keymanagerConfig()).getCN();
    } else {
      LOG_ERROR << "Unknown provisioning method";
      return false;
    }
  }

  storage->storeDeviceId(device_id);
  return true;
}
void Repository::resetDeviceId() { storage->clearDeviceId(); }

void Repository::setEcuSerialMembers(const std::pair<std::string, std::string>& ecu_serials) {
  primary_ecu_serial = ecu_serials.first;
  primary_hardware_id_ = ecu_serials.second;
}

// Postcondition [(serial, hw_id)] is in the storage
bool Repository::initEcuSerials(const UptaneConfig& uptane_config) {
  std::vector<std::pair<std::string, std::string> > ecu_serials;

  // TODO: the assumption now is that the set of connected ECUs doesn't change, but it might obviously
  //   not be the case. ECU discovery seems to be a big story and should be worked on accordingly.
  if (storage->loadEcuSerials(&ecu_serials)) {
    setEcuSerialMembers(ecu_serials[0]);
    return true;
  }

  std::string primary_ecu_serial_local = uptane_config.primary_ecu_serial;
  if (primary_ecu_serial_local.empty()) {
    primary_ecu_serial_local = Crypto::getKeyId(keys_.getUptanePublicKey());
  }

  std::string primary_ecu_hardware_id = uptane_config.primary_ecu_hardware_id;
  if (primary_ecu_hardware_id.empty()) {
    primary_ecu_hardware_id = Utils::getHostname();
    if (primary_ecu_hardware_id == "") {
      return false;
    }
  }

  ecu_serials.emplace_back(primary_ecu_serial_local, primary_ecu_hardware_id);

  std::vector<std::shared_ptr<Uptane::SecondaryInterface> >::const_iterator it;
  for (it = secondary_info.begin(); it != secondary_info.end(); ++it) {
    ecu_serials.emplace_back((*it)->getSerial(), (*it)->getHwId());
  }

  storage->storeEcuSerials(ecu_serials);
  setEcuSerialMembers(ecu_serials[0]);
  return true;
}

void Repository::resetEcuSerials() {
  storage->clearEcuSerials();
  primary_ecu_serial = "";
}

// Postcondition: (public, private) is in the storage. It should not be stored until secondaries are provisioned
bool Repository::initPrimaryEcuKeys() { return keys_.generateUptaneKeyPair().size() != 0u; }

void Repository::resetEcuKeys() { storage->clearPrimaryKeys(); }

bool Repository::loadSetTlsCreds() {
  KeyManager keys(storage, config.keymanagerConfig());
  keys.copyCertsToCurl(&http);
  return keys.isOk();
}

// Postcondition: TLS credentials are in the storage
InitRetCode Repository::initTlsCreds(const ProvisionConfig& provision_config) {
  if (loadSetTlsCreds()) {
    return INIT_RET_OK;
  }

  if (provision_config.mode != kAutomatic) {
    LOG_ERROR << "Credentials not found";
    return INIT_RET_STORAGE_FAILURE;
  }

  // Autoprovision is needed and possible => autoprovision

  // set bootstrap credentials
  Bootstrap boot(provision_config.provision_path, provision_config.p12_password);
  http.setCerts(boot.getCa(), kFile, boot.getCert(), kFile, boot.getPkey(), kFile);

  Json::Value data;
  std::string device_id;
  if (!storage->loadDeviceId(&device_id)) {
    LOG_ERROR << "device_id unknown during autoprovisioning process";
    return INIT_RET_STORAGE_FAILURE;
  }
  data["deviceId"] = device_id;
  data["ttl"] = provision_config.expiry_days;
  HttpResponse response = http.post(provision_config.server + "/devices", data);
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

void Repository::resetTlsCreds() {
  if (config.provision.mode != kImplicit) {
    storage->clearTlsCreds();
  }
}

// Postcondition: "ECUs registered" flag set in the storage
InitRetCode Repository::initEcuRegister() {
  if (storage->loadEcuRegistered()) {
    return INIT_RET_OK;
  }

  std::string primary_public = keys_.getUptanePublicKey();

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
    primary_ecu["clientKey"]["keytype"] = config.uptane.getKeyTypeString();
    primary_ecu["clientKey"]["keyval"]["public"] = primary_public;
    all_ecus["ecus"].append(primary_ecu);
  }

  std::vector<std::shared_ptr<Uptane::SecondaryInterface> >::const_iterator it;
  for (it = secondary_info.begin(); it != secondary_info.end(); it++) {
    Json::Value ecu;
    auto public_key = (*it)->getPublicKey();
    ecu["hardware_identifier"] = (*it)->getHwId();
    ecu["ecu_serial"] = (*it)->getSerial();
    ecu["clientKey"]["keytype"] = keyTypeToString(public_key.first);
    ecu["clientKey"]["keyval"]["public"] = public_key.second;
    all_ecus["ecus"].append(ecu);
  }

  HttpResponse response = http.post(config.uptane.director_server + "/ecus", all_ecus);
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
bool Repository::initialize() {
  for (int i = 0; i < MaxInitializationAttempts; i++) {
    if (!initDeviceId(config.provision, config.uptane)) {
      LOG_ERROR << "Device ID generation failed, abort initialization";
      return false;
    }

    InitRetCode ret_code = initTlsCreds(config.provision);
    // if a device with the same ID has already been registered to the server,
    // generate a new one
    if (ret_code == INIT_RET_OCCUPIED) {
      resetDeviceId();
      LOG_INFO << "Device name is already registered, restart";
      continue;
    }
    if (ret_code != INIT_RET_OK) {
      LOG_ERROR << "Autoprovisioning failed, abort initialization";
      return false;
    }

    if (!initPrimaryEcuKeys()) {
      LOG_ERROR << "ECU key generation failed, abort initialization";
      return false;
    }
    if (!initEcuSerials(config.uptane)) {
      LOG_ERROR << "ECU serial generation failed, abort initialization";
      return false;
    }

    ret_code = initEcuRegister();
    // if ECUs with same ID have been registered to the server, we don't have a
    // clear remediation path right now, just ignore the error
    if (ret_code == INIT_RET_OCCUPIED) {
      LOG_INFO << "ECU serial is already registered";
    } else if (ret_code != INIT_RET_OK) {
      LOG_ERROR << "ECU registration failed, abort initialization";
      return false;
    }

    // TODO: acknowledge on server _before_ setting the flag
    storage->storeEcuRegistered();
    return true;
  }
  LOG_ERROR << "Initialization failed after " << MaxInitializationAttempts << " attempts";
  return false;
}

void Repository::initReset() {
  storage->clearEcuRegistered();
  resetTlsCreds();
  resetEcuKeys();
  resetEcuSerials();
  resetDeviceId();
}
}  // namespace Uptane
