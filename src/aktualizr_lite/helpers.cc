#include "helpers.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "package_manager/ostreemanager.h"

static void finalizeIfNeeded(INvStorage &storage, PackageConfig &config) {
  boost::optional<Uptane::Target> pending_version;
  storage.loadInstalledVersions("", nullptr, &pending_version);

  if (!!pending_version) {
    GObjectUniquePtr<OstreeSysroot> sysroot_smart = OstreeManager::LoadSysroot(config.sysroot);
    OstreeDeployment *booted_deployment = ostree_sysroot_get_booted_deployment(sysroot_smart.get());
    if (booted_deployment == nullptr) {
      throw std::runtime_error("Could not get booted deployment in " + config.sysroot.string());
    }
    std::string current_hash = ostree_deployment_get_csum(booted_deployment);

    const Uptane::Target &target = *pending_version;
    if (current_hash == target.sha256Hash()) {
      LOG_INFO << "Marking target install complete for: " << target;
      storage.saveInstalledVersion("", target, InstalledVersionUpdateMode::kCurrent);
    }
  }
}

LiteClient::LiteClient(Config &config_in)
    : config(std::move(config_in)), primary_ecu(Uptane::EcuSerial::Unknown(), "") {
  std::string pkey;
  storage = INvStorage::newStorage(config.storage);
  storage->importData(config.import);

  EcuSerials ecu_serials;
  if (!storage->loadEcuSerials(&ecu_serials)) {
    // Set a "random" serial so we don't get warning messages.
    std::string serial = config.provision.primary_ecu_serial;
    std::string hwid = config.provision.primary_ecu_hardware_id;
    if (hwid.empty()) {
      hwid = Utils::getHostname();
    }
    if (serial.empty()) {
      boost::uuids::uuid tmp = boost::uuids::random_generator()();
      serial = boost::uuids::to_string(tmp);
    }
    ecu_serials.emplace_back(Uptane::EcuSerial(serial), Uptane::HardwareIdentifier(hwid));
    storage->storeEcuSerials(ecu_serials);
  }
  primary_ecu = ecu_serials[0];

  auto http_client = std::make_shared<HttpClient>();

  KeyManager keys(storage, config.keymanagerConfig());
  keys.copyCertsToCurl(*http_client);

  primary = std::make_shared<SotaUptaneClient>(config, storage, http_client);
  primary->initializePrimaryEcu();
  finalizeIfNeeded(*storage, config.pacman);
}
