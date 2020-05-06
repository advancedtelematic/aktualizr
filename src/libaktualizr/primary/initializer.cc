#include "initializer.h"

#include <string>

#include <openssl/bio.h>
#include <boost/scoped_array.hpp>

#include "bootstrap/bootstrap.h"
#include "crypto/keymanager.h"
#include "logging/logging.h"
#include "storage/invstorage.h"

// Postcondition: device_id is in the storage
void Initializer::initDeviceId() {
  // if device_id is already stored, just return
  std::string device_id;
  if (storage_->loadDeviceId(&device_id)) {
    return;
  }

  // if device_id is specified in config, just use it, otherwise generate a  random one
  device_id = config_.device_id;
  if (device_id.empty()) {
    if (config_.mode == ProvisionMode::kSharedCred) {
      device_id = Utils::genPrettyName();
    } else if (config_.mode == ProvisionMode::kDeviceCred) {
      device_id = keys_.getCN();
    } else {
      throw Error("Unknown provisioning method");
    }
  }

  storage_->storeDeviceId(device_id);
}

void Initializer::resetDeviceId() { storage_->clearDeviceId(); }

// Postcondition [(serial, hw_id)] is in the storage
void Initializer::initEcuSerials() {
  EcuSerials ecu_serials;

  // TODO: the assumption now is that the set of connected ECUs doesn't change, but it might obviously
  //   not be the case. ECU discovery seems to be a big story and should be worked on accordingly.
  if (storage_->loadEcuSerials(&ecu_serials)) {
    return;
  }

  std::string primary_ecu_serial_local = config_.primary_ecu_serial;
  if (primary_ecu_serial_local.empty()) {
    primary_ecu_serial_local = keys_.UptanePublicKey().KeyId();
  }

  std::string primary_ecu_hardware_id = config_.primary_ecu_hardware_id;
  if (primary_ecu_hardware_id.empty()) {
    primary_ecu_hardware_id = Utils::getHostname();
    if (primary_ecu_hardware_id == "") {
      throw Error("Could not get current host name, please configure an hardware ID explicitely");
    }
  }

  ecu_serials.emplace_back(Uptane::EcuSerial(primary_ecu_serial_local),
                           Uptane::HardwareIdentifier(primary_ecu_hardware_id));

  for (const auto& s : secondaries_) {
    ecu_serials.emplace_back(s.first, s.second->getHwId());
  }

  storage_->storeEcuSerials(ecu_serials);
}

void Initializer::resetEcuSerials() { storage_->clearEcuSerials(); }

// Postcondition: (public, private) is in the storage. It should not be stored until secondaries are provisioned
void Initializer::initPrimaryEcuKeys() {
  std::string key_pair;
  try {
    key_pair = keys_.generateUptaneKeyPair();
  } catch (const std::exception& e) {
    throw KeyGenerationError(e.what());
  }

  if (key_pair.size() == 0U) {
    throw KeyGenerationError("Unknow error");
  }
}

void Initializer::resetEcuKeys() { storage_->clearPrimaryKeys(); }

bool Initializer::loadSetTlsCreds() {
  keys_.copyCertsToCurl(*http_client_);
  return keys_.isOk();
}

// Postcondition: TLS credentials are in the storage
void Initializer::initTlsCreds() {
  if (loadSetTlsCreds()) {
    return;
  }

  if (config_.mode != ProvisionMode::kSharedCred) {
    throw StorageError("Shared credentials expected but not found");
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
    throw StorageError("Unable to load device_id during shared credential provisioning");
  }
  data["deviceId"] = device_id;
  data["ttl"] = config_.expiry_days;
  HttpResponse response = http_client_->post(config_.server + "/devices", data);
  if (!response.isOk()) {
    Json::Value resp_code = response.getJson()["code"];
    if (resp_code.isString() && resp_code.asString() == "device_already_registered") {
      LOG_ERROR << "Device ID " << device_id << " is already registered.";
      throw ServerOccupied();
    }
    const auto err = std::string("Shared credential provisioning failed: ") +
                     std::to_string(response.http_status_code) + " " + response.body;
    throw ServerError(err);
  }

  std::string pkey;
  std::string cert;
  std::string ca;
  StructGuard<BIO> device_p12(BIO_new_mem_buf(response.body.c_str(), static_cast<int>(response.body.size())),
                              BIO_vfree);
  if (!Crypto::parseP12(device_p12.get(), "", &pkey, &cert, &ca)) {
    throw ServerError("Received malformed device credentials from the server");
  }
  storage_->storeTlsCreds(ca, cert, pkey);

  // set provisioned credentials
  if (!loadSetTlsCreds()) {
    throw Error("Failed to configure HTTP client with device credentials.");
  }

  LOG_INFO << "Provisioned successfully on Device Gateway.";
}

void Initializer::resetTlsCreds() {
  if (config_.mode != ProvisionMode::kDeviceCred) {
    storage_->clearTlsCreds();
  }
}

// Postcondition: "ECUs registered" flag set in the storage
void Initializer::initEcuRegister() {
  if (storage_->loadEcuRegistered()) {
    return;
  }

  PublicKey uptane_public_key = keys_.UptanePublicKey();

  if (uptane_public_key.Type() == KeyType::kUnknown) {
    throw StorageError("Invalid key in storage");
  }

  EcuSerials ecu_serials;
  // initEcuSerials should have been called by this point
  if (!storage_->loadEcuSerials(&ecu_serials) || ecu_serials.size() < 1) {
    throw StorageError("Could not load ECUs from storage");
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
    SecondaryInterface& itf = *sec.second;

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
      throw ServerError("One or more ECUs are unexpectedly already registered");
    }
    const auto err =
        std::string("Error registering device: ") + std::to_string(response.http_status_code) + " " + response.body;
    throw ServerError(err);
  }

  LOG_INFO << "ECUs have been successfully registered with the server.";
}

void Initializer::initEcuReportCounter() {
  std::vector<std::pair<Uptane::EcuSerial, int64_t>> ecu_cnt;

  if (storage_->loadEcuReportCounter(&ecu_cnt)) {
    return;
  }

  EcuSerials ecu_serials;

  if (!storage_->loadEcuSerials(&ecu_serials) || (ecu_serials.size() == 0)) {
    throw Error("Could not load ECU serials");
  }

  storage_->saveEcuReportCounter(Uptane::EcuSerial(ecu_serials[0].first.ToString()), 0);
}

// Postcondition: "ECUs registered" flag set in the storage
Initializer::Initializer(const ProvisionConfig& config_in, std::shared_ptr<INvStorage> storage_in,
                         std::shared_ptr<HttpInterface> http_client_in, KeyManager& keys_in,
                         const std::map<Uptane::EcuSerial, std::shared_ptr<SecondaryInterface>>& secondaries_in)
    : config_(config_in),
      storage_(std::move(storage_in)),
      http_client_(std::move(http_client_in)),
      keys_(keys_in),
      secondaries_(secondaries_in) {
  for (int i = 0; i < MaxInitializationAttempts; i++) {
    initDeviceId();

    try {
      initTlsCreds();
    } catch (const ServerOccupied& e) {
      // if a device with the same ID has already been registered to the server,
      // generate a new one
      resetDeviceId();
      LOG_ERROR << "Device name is already registered. Retrying.";
      continue;
    }

    initPrimaryEcuKeys();

    initEcuSerials();

    initEcuReportCounter();

    initEcuRegister();

    storage_->storeEcuRegistered();

    return;
  }

  throw Error(std::string("Initialization failed after ") + std::to_string(MaxInitializationAttempts) + " attempts");
}
