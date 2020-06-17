#include "initializer.h"

#include <string>

#include <openssl/bio.h>
#include <boost/scoped_array.hpp>

#include "bootstrap/bootstrap.h"
#include "crypto/keymanager.h"
#include "logging/logging.h"

// Postcondition: device_id is in the storage
void Initializer::initDeviceId() {
  // If device_id is already stored, just return.
  std::string device_id;
  if (storage_->loadDeviceId(&device_id)) {
    return;
  }

  // If device_id is specified in the config, use that.
  device_id = config_.device_id;
  if (device_id.empty()) {
    // Otherwise, try to read the device certificate if it is available.
    try {
      device_id = keys_.getCN();
    } catch (const std::exception& e) {
      // No certificate: for device credential provisioning, abort. For shared
      // credential provisioning, generate a random name.
      if (config_.mode == ProvisionMode::kSharedCred) {
        device_id = Utils::genPrettyName();
      } else if (config_.mode == ProvisionMode::kDeviceCred) {
        throw e;
      } else {
        throw Error("Unknown provisioning method");
      }
    }
  }

  storage_->storeDeviceId(device_id);
}

void Initializer::resetDeviceId() { storage_->clearDeviceId(); }

// Postcondition [(serial, hw_id)] is in the storage
void Initializer::initEcuSerials() {
  EcuSerials stored_ecu_serials;
  storage_->loadEcuSerials(&stored_ecu_serials);

  std::string primary_ecu_serial_local = config_.primary_ecu_serial;
  if (primary_ecu_serial_local.empty()) {
    primary_ecu_serial_local = keys_.UptanePublicKey().KeyId();
  }

  std::string primary_ecu_hardware_id = config_.primary_ecu_hardware_id;
  if (primary_ecu_hardware_id.empty()) {
    primary_ecu_hardware_id = Utils::getHostname();
    if (primary_ecu_hardware_id == "") {
      throw Error("Could not get current host name, please configure an hardware ID explicitly");
    }
  }

  new_ecu_serials_.emplace_back(Uptane::EcuSerial(primary_ecu_serial_local),
                                Uptane::HardwareIdentifier(primary_ecu_hardware_id));
  for (const auto& s : secondaries_) {
    new_ecu_serials_.emplace_back(s.first, s.second->getHwId());
  }

  register_ecus_ = stored_ecu_serials.empty();
  if (!stored_ecu_serials.empty()) {
    // We should probably clear the misconfigured_ecus table once we have
    // consent working.
    std::vector<bool> found(stored_ecu_serials.size(), false);

    EcuCompare primary_comp(new_ecu_serials_[0]);
    EcuSerials::const_iterator store_it;
    store_it = std::find_if(stored_ecu_serials.cbegin(), stored_ecu_serials.cend(), primary_comp);
    if (store_it == stored_ecu_serials.cend()) {
      LOG_INFO << "Configured Primary ECU serial " << new_ecu_serials_[0].first << " with hardware ID "
               << new_ecu_serials_[0].second << " not found in storage.";
      register_ecus_ = true;
    } else {
      found[static_cast<size_t>(store_it - stored_ecu_serials.cbegin())] = true;
    }

    // Check all configured Secondaries to see if any are new.
    for (auto it = secondaries_.cbegin(); it != secondaries_.cend(); ++it) {
      EcuCompare secondary_comp(std::make_pair(it->second->getSerial(), it->second->getHwId()));
      store_it = std::find_if(stored_ecu_serials.cbegin(), stored_ecu_serials.cend(), secondary_comp);
      if (store_it == stored_ecu_serials.cend()) {
        LOG_INFO << "Configured Secondary ECU serial " << it->second->getSerial() << " with hardware ID "
                 << it->second->getHwId() << " not found in storage.";
        register_ecus_ = true;
      } else {
        found[static_cast<size_t>(store_it - stored_ecu_serials.cbegin())] = true;
      }
    }

    // Check all stored Secondaries not already matched to see if any have been
    // removed. Store them in a separate table to keep track of them.
    std::vector<bool>::iterator found_it;
    for (found_it = found.begin(); found_it != found.end(); ++found_it) {
      if (!*found_it) {
        auto not_registered = stored_ecu_serials[static_cast<size_t>(found_it - found.begin())];
        LOG_INFO << "ECU serial " << not_registered.first << " with hardware ID " << not_registered.second
                 << " in storage was not found in Secondary configuration.";
        register_ecus_ = true;
        storage_->saveMisconfiguredEcu({not_registered.first, not_registered.second, EcuState::kOld});
      }
    }
  }
}

// Postcondition: (public, private) is in the storage. It should not be stored until Secondaries are provisioned
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
  // Allow re-registration if the ECUs have changed.
  if (!register_ecus_) {
    LOG_DEBUG << "All ECUs are already registered with the server.";
    return;
  }

  PublicKey uptane_public_key = keys_.UptanePublicKey();

  if (uptane_public_key.Type() == KeyType::kUnknown) {
    throw StorageError("Invalid key in storage");
  }

  Json::Value all_ecus;
  all_ecus["primary_ecu_serial"] = new_ecu_serials_[0].first.ToString();
  all_ecus["ecus"] = Json::arrayValue;
  {
    Json::Value primary_ecu;
    primary_ecu["hardware_identifier"] = new_ecu_serials_[0].second.ToString();
    primary_ecu["ecu_serial"] = new_ecu_serials_[0].first.ToString();
    primary_ecu["clientKey"] = keys_.UptanePublicKey().ToUptane();
    all_ecus["ecus"].append(primary_ecu);
  }

  for (const auto& info : sec_info_) {
    Json::Value ecu;
    ecu["hardware_identifier"] = info.hw_id.ToString();
    ecu["ecu_serial"] = info.serial.ToString();
    ecu["clientKey"] = info.pub_key.ToUptane();
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

  // Only store the changes if we successfully registered the ECUs.
  storage_->storeEcuSerials(new_ecu_serials_);
  for (const auto& info : sec_info_) {
    storage_->saveSecondaryInfo(info.serial, info.type, info.pub_key);
  }
  storage_->storeEcuRegistered();

  LOG_INFO << "ECUs have been successfully registered with the server.";
}

void Initializer::initSecondaryInfo() {
  for (const auto& s : secondaries_) {
    const Uptane::EcuSerial serial = s.first;
    Uptane::SecondaryInterface& sec = *s.second;

    SecondaryInfo info;
    // If upgrading from the older version of the storage without the
    // secondary_ecus table, we need to migrate the data. This should be done
    // regardless of whether we need to (re-)register the ECUs.
    // The ECU serials should be already initialized by this point.
    if (!storage_->loadSecondaryInfo(serial, &info) || info.type == "" || info.pub_key.Type() == KeyType::kUnknown) {
      info.serial = serial;
      info.hw_id = sec.getHwId();
      info.type = sec.Type();
      const PublicKey& p = sec.getPublicKey();
      if (p.Type() != KeyType::kUnknown) {
        info.pub_key = p;
      }
      // If we don't need to register the ECUs, we still need to store this info
      // to complete the migration.
      if (!register_ecus_) {
        storage_->saveSecondaryInfo(info.serial, info.type, info.pub_key);
      }
    }
    // We will need this info later if the device is not yet provisioned
    sec_info_.push_back(std::move(info));
  }
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
                         const std::map<Uptane::EcuSerial, std::shared_ptr<Uptane::SecondaryInterface>>& secondaries_in)
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

    initSecondaryInfo();

    initEcuRegister();

    initEcuReportCounter();

    return;
  }

  throw Error(std::string("Initialization failed after ") + std::to_string(MaxInitializationAttempts) + " attempts");
}
