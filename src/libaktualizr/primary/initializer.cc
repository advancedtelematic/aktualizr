#include "initializer.h"

#include <string>

#include <openssl/bio.h>
#include <boost/scoped_array.hpp>

#include "bootstrap/bootstrap.h"
#include "crypto/keymanager.h"
#include "logging/logging.h"
#include "storage/invstorage.h"

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
    if (config_.mode == ProvisionMode::kSharedCred) {
      device_id = Utils::genPrettyName();
    } else if (config_.mode == ProvisionMode::kDeviceCred) {
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

  ecu_serials.emplace_back(Uptane::EcuSerial(primary_ecu_serial_local),
                           Uptane::HardwareIdentifier(primary_ecu_hardware_id));

  for (const auto& s : secondaries_) {
    ecu_serials.emplace_back(s.first, s.second->getHwId());
  }

  storage_->storeEcuSerials(ecu_serials);
  return true;
}

void Initializer::resetEcuSerials() { storage_->clearEcuSerials(); }

// Postcondition: (public, private) is in the storage. It should not be stored until secondaries are provisioned
bool Initializer::initPrimaryEcuKeys() { return keys_.generateUptaneKeyPair().size() != 0u; }

void Initializer::resetEcuKeys() { storage_->clearPrimaryKeys(); }

bool Initializer::loadSetTlsCreds() {
  keys_.copyCertsToCurl(*http_client_);
  return keys_.isOk();
}

// Postcondition: TLS credentials are in the storage
InitRetCode Initializer::initTlsCreds() {
  if (loadSetTlsCreds()) {
    return InitRetCode::kOk;
  }

  if (config_.mode != ProvisionMode::kSharedCred) {
    LOG_ERROR << "Credentials not found";
    return InitRetCode::kStorageFailure;
  }

  // Shared credential provision is required and possible => (automatically)
  // provision with shared credentials.

  // set bootstrap credentials
  Bootstrap boot(config_.provision_path, config_.p12_password);
  http_client_->setCerts(boot.getCa(), CryptoSource::kFile, boot.getCert(), CryptoSource::kFile, boot.getPkey(),
                         CryptoSource::kFile);

  Json::Value data;
  std::string device_id;
  if (!storage_->loadDeviceId(&device_id)) {
    LOG_ERROR << "Unknown device_id during shared credential provisioning.";
    return InitRetCode::kStorageFailure;
  }
  data["deviceId"] = device_id;
  data["ttl"] = config_.expiry_days;
  HttpResponse response = http_client_->post(config_.server + "/devices", data);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() && resp_code.asString() == "device_already_registered") {
      LOG_ERROR << "Device id" << device_id << "is occupied";
      return InitRetCode::kOccupied;
    }
    LOG_ERROR << "Shared credential provisioning failed, response: " << response.body;
    return InitRetCode::kServerFailure;
  }

  std::string pkey;
  std::string cert;
  std::string ca;
  StructGuard<BIO> device_p12(BIO_new_mem_buf(response.body.c_str(), static_cast<int>(response.body.size())),
                              BIO_vfree);
  if (!Crypto::parseP12(device_p12.get(), "", &pkey, &cert, &ca)) {
    LOG_ERROR << "Received a malformed P12 package from the server";
    return InitRetCode::kBadP12;
  }
  storage_->storeTlsCreds(ca, cert, pkey);

  // set provisioned credentials
  if (!loadSetTlsCreds()) {
    LOG_ERROR << "Failed to set provisioned credentials";
    return InitRetCode::kStorageFailure;
  }

  LOG_INFO << "Provisioned successfully on Device Gateway";
  return InitRetCode::kOk;
}

void Initializer::resetTlsCreds() {
  if (config_.mode != ProvisionMode::kDeviceCred) {
    storage_->clearTlsCreds();
  }
}

// Postcondition: "ECUs registered" flag set in the storage
InitRetCode Initializer::initEcuRegister() {
  if (storage_->loadEcuRegistered()) {
    return InitRetCode::kOk;
  }

  PublicKey uptane_public_key = keys_.UptanePublicKey();

  if (uptane_public_key.Type() == KeyType::kUnknown) {
    return InitRetCode::kStorageFailure;
  }

  EcuSerials ecu_serials;
  // initEcuSerials should have been called by this point
  if (!storage_->loadEcuSerials(&ecu_serials) || ecu_serials.size() < 1) {
    return InitRetCode::kStorageFailure;
  }

  Json::Value all_ecus;
  all_ecus["primary_ecu_serial"] = ecu_serials[0].first.ToString();
  all_ecus["ecus"] = Json::arrayValue;
  {
    Json::Value primary_ecu;
    primary_ecu["hardware_identifier"] = ecu_serials[0].second.ToString();
    primary_ecu["ecu_serial"] = ecu_serials[0].first.ToString();
    primary_ecu["clientKey"] = keys_.UptanePublicKey().ToUptane();
    all_ecus["ecus"].append(primary_ecu);
  }

  for (const auto& sec : secondaries_) {
    const Uptane::EcuSerial ecu_serial = sec.first;
    Uptane::SecondaryInterface& itf = *sec.second;

    const Uptane::HardwareIdentifier hw_id = itf.getHwId();
    const PublicKey pub_key = itf.getPublicKey();
    storage_->saveSecondaryInfo(ecu_serial, itf.Type(), pub_key);

    Json::Value ecu;
    ecu["hardware_identifier"] = hw_id.ToString();
    ecu["ecu_serial"] = ecu_serial.ToString();
    ecu["clientKey"] = pub_key.ToUptane();
    all_ecus["ecus"].append(ecu);
  }

  HttpResponse response = http_client_->post(config_.ecu_registration_endpoint, all_ecus);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() &&
        (resp_code.asString() == "ecu_already_registered" || resp_code.asString() == "device_already_registered")) {
      LOG_ERROR << "One or more ECUs are unexpectedly already registered.";
      return InitRetCode::kOccupied;
    }
    LOG_ERROR << "Error registering device on Uptane, response: " << response.body;
    return InitRetCode::kServerFailure;
  }
  // do not call storage_->storeEcuRegistered(), it will be called from the top-level Init function after the
  // acknowledgement
  LOG_INFO << "ECUs have been successfully registered to the server.";
  return InitRetCode::kOk;
}

bool Initializer::initEcuReportCounter() {
  std::vector<std::pair<Uptane::EcuSerial, int64_t>> ecu_cnt;

  if (storage_->loadEcuReportCounter(&ecu_cnt)) {
    return true;
  }

  EcuSerials ecu_serials;

  if (!storage_->loadEcuSerials(&ecu_serials) || (ecu_serials.size() == 0)) {
    return false;
  }

  storage_->saveEcuReportCounter(Uptane::EcuSerial(ecu_serials[0].first.ToString()), 0);

  return true;
}
// Postcondition: "ECUs registered" flag set in the storage
Initializer::Initializer(const ProvisionConfig& config_in, std::shared_ptr<INvStorage> storage_in,
                         std::shared_ptr<HttpInterface> http_client_in, KeyManager& keys_in,
                         const std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface>>& secondaries_in)
    : config_(config_in),
      storage_(std::move(storage_in)),
      http_client_(std::move(http_client_in)),
      keys_(keys_in),
      secondaries_(secondaries_in) {
  success_ = false;
  for (int i = 0; i < MaxInitializationAttempts; i++) {
    if (!initDeviceId()) {
      LOG_ERROR << "Device ID generation failed. Aborting initialization.";
      return;
    }

    InitRetCode ret_code = initTlsCreds();
    // if a device with the same ID has already been registered to the server,
    // generate a new one
    if (ret_code == InitRetCode::kOccupied) {
      resetDeviceId();
      LOG_INFO << "Device name is already registered. Restarting.";
      continue;
    } else if (ret_code == InitRetCode::kStorageFailure) {
      LOG_ERROR << "Error reading existing provisioning data from storage.";
      return;
    } else if (ret_code != InitRetCode::kOk) {
      LOG_ERROR << "Shared credential provisioning failed. Aborting initialization.";
      return;
    }

    if (!initPrimaryEcuKeys()) {
      LOG_ERROR << "ECU key generation failed. Aborting initialization.";
      return;
    }
    if (!initEcuSerials()) {
      LOG_ERROR << "ECU serial generation failed. Aborting initialization.";
      return;
    }
    if (!initEcuReportCounter()) {
      LOG_ERROR << "ECU report counter init failed. Aborting initialization.";
      return;
    }

    ret_code = initEcuRegister();
    // if ECUs with same ID have been registered to the server, we don't have a
    // clear remediation path right now, just ignore the error
    if (ret_code == InitRetCode::kOccupied) {
      LOG_INFO << "ECU serial is already registered.";
    } else if (ret_code != InitRetCode::kOk) {
      LOG_ERROR << "ECU registration failed. Aborting initialization.";
      return;
    }

    // TODO: acknowledge on server _before_ setting the flag
    storage_->storeEcuRegistered();
    success_ = true;
    return;
  }
  LOG_ERROR << "Initialization failed after " << MaxInitializationAttempts << " attempts.";
}
