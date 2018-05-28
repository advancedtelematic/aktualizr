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
  if (storage_->loadDeviceId(&device_id)) {
    return true;
  }

  // if device_id is specified in config, just use it, otherwise generate a  random one
  device_id = config_.device_id;
  if (device_id.empty()) {
    if (config_.mode == kAutomatic) {
      device_id = Utils::genPrettyName();
    } else if (config_.mode == kImplicit) {
      device_id = keys_.getCN();
    } else {
      LOG_ERROR << "Unknown provisioning method";
      return false;
    }
  }

  storage_->storeDeviceId(device_id);
  return true;
}
void Initializer::resetDeviceId() { storage_->clearDeviceId(); }

// Postcondition [(serial, hw_id)] is in the storage
bool Initializer::initEcuSerials() {
  EcuSerials ecu_serials;

  // TODO: the assumption now is that the set of connected ECUs doesn't change, but it might obviously
  //   not be the case. ECU discovery seems to be a big story and should be worked on accordingly.
  if (storage_->loadEcuSerials(&ecu_serials)) {
    return true;
  }

  std::string primary_ecu_serial_local = config_.primary_ecu_serial;
  if (primary_ecu_serial_local.empty()) {
    primary_ecu_serial_local = keys_.UptanePublicKey().KeyId();
  }

  std::string primary_ecu_hardware_id = config_.primary_ecu_hardware_id;
  if (primary_ecu_hardware_id.empty()) {
    primary_ecu_hardware_id = Utils::getHostname();
    if (primary_ecu_hardware_id == "") {
      return false;
    }
  }

  ecu_serials.emplace_back(primary_ecu_serial_local, Uptane::HardwareIdentifier(primary_ecu_hardware_id));

  for (auto it = secondary_info_.begin(); it != secondary_info_.end(); ++it) {
    ecu_serials.emplace_back(it->second->getSerial(), it->second->getHwId());
  }

  storage_->storeEcuSerials(ecu_serials);
  return true;
}

void Initializer::resetEcuSerials() { storage_->clearEcuSerials(); }

// Postcondition: (public, private) is in the storage. It should not be stored until secondaries are provisioned
bool Initializer::initPrimaryEcuKeys() { return keys_.generateUptaneKeyPair().size() != 0u; }

void Initializer::resetEcuKeys() { storage_->clearPrimaryKeys(); }

bool Initializer::loadSetTlsCreds() {
  keys_.copyCertsToCurl(&http_client_);
  return keys_.isOk();
}

// Postcondition: TLS credentials are in the storage
InitRetCode Initializer::initTlsCreds() {
  if (loadSetTlsCreds()) {
    return INIT_RET_OK;
  }

  if (config_.mode != kAutomatic) {
    LOG_ERROR << "Credentials not found";
    return INIT_RET_STORAGE_FAILURE;
  }

  // Autoprovision is needed and possible => autoprovision

  // set bootstrap credentials
  Bootstrap boot(config_.provision_path, config_.p12_password);
  http_client_.setCerts(boot.getCa(), kFile, boot.getCert(), kFile, boot.getPkey(), kFile);

  Json::Value data;
  std::string device_id;
  if (!storage_->loadDeviceId(&device_id)) {
    LOG_ERROR << "device_id unknown during autoprovisioning process";
    return INIT_RET_STORAGE_FAILURE;
  }
  data["deviceId"] = device_id;
  data["ttl"] = config_.expiry_days;
  HttpResponse response = http_client_.post(config_.server + "/devices", data);
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
  storage_->storeTlsCreds(ca, cert, pkey);

  // set provisioned credentials
  if (!loadSetTlsCreds()) {
    LOG_ERROR << "Failed to set provisioned credentials";
    return INIT_RET_STORAGE_FAILURE;
  }

  LOG_INFO << "Provisioned successfully on Device Gateway";
  return INIT_RET_OK;
}

void Initializer::resetTlsCreds() {
  if (config_.mode != kImplicit) {
    storage_->clearTlsCreds();
  }
}

// Postcondition: "ECUs registered" flag set in the storage
InitRetCode Initializer::initEcuRegister() {
  if (storage_->loadEcuRegistered()) {
    return INIT_RET_OK;
  }

  PublicKey uptane_public_key = keys_.UptanePublicKey();

  if (uptane_public_key.Type() == kUnknownKey) {
    return INIT_RET_STORAGE_FAILURE;
  }

  EcuSerials ecu_serials;
  // initEcuSerials should have been called by this point
  if (!storage_->loadEcuSerials(&ecu_serials) || ecu_serials.size() < 1) {
    return INIT_RET_STORAGE_FAILURE;
  }

  Json::Value all_ecus;
  all_ecus["primary_ecu_serial"] = ecu_serials[0].first;
  all_ecus["ecus"] = Json::arrayValue;
  {
    Json::Value primary_ecu;
    primary_ecu["hardware_identifier"] = ecu_serials[0].second.ToString();
    primary_ecu["ecu_serial"] = ecu_serials[0].first;
    primary_ecu["clientKey"] = keys_.UptanePublicKey().ToUptane();
    all_ecus["ecus"].append(primary_ecu);
  }

  for (auto it = secondary_info_.cbegin(); it != secondary_info_.cend(); it++) {
    Json::Value ecu;
    auto public_key = it->second->getPublicKey();
    ecu["hardware_identifier"] = it->second->getHwId().ToString();
    ecu["ecu_serial"] = it->second->getSerial();
    ecu["clientKey"] = public_key.ToUptane();
    all_ecus["ecus"].append(ecu);
  }

  HttpResponse response = http_client_.post(config_.ecu_registration_endpoint, all_ecus);
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
  // do not call storage_->storeEcuRegistered(), it will be called from the top-level Init function after the
  // acknowledgement
  LOG_INFO << "ECUs have been successfully registered to the server";
  return INIT_RET_OK;
}

// Postcondition: "ECUs registered" flag set in the storage
Initializer::Initializer(const ProvisionConfig& config, std::shared_ptr<INvStorage> storage, HttpInterface& http_client,
                         KeyManager& keys,
                         const std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >& secondary_info)
    : config_(config),
      storage_(std::move(storage)),
      http_client_(http_client),
      keys_(keys),
      secondary_info_(secondary_info) {
  success_ = false;
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
    storage_->storeEcuRegistered();
    success_ = true;
    return;
  }
  LOG_ERROR << "Initialization failed after " << MaxInitializationAttempts << " attempts";
}
