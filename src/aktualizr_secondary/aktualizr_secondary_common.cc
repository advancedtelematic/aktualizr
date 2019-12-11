#include "aktualizr_secondary_common.h"
#include "package_manager/packagemanagerfactory.h"
#include "utilities/utils.h"

AktualizrSecondaryCommon::AktualizrSecondaryCommon(const AktualizrSecondaryConfig &config,
                                                   std::shared_ptr<INvStorage> storage)
    : config_(config),
      storage_(std::move(storage)),
      keys_(storage_, config.keymanagerConfig()),
      ecu_serial_(Uptane::EcuSerial::Unknown()),
      hardware_id_(Uptane::HardwareIdentifier::Unknown()) {
  pacman = PackageManagerFactory::makePackageManager(config_.pacman, config_.bootloader, storage_, nullptr);

  // Load Root keys from storage
  std::string root;
  storage_->loadLatestRoot(&root, Uptane::RepositoryType::Director());
  if (root.size() > 0) {
    LOG_DEBUG << "Loading root.json:" << root;
    root_ = Uptane::Root(Uptane::RepositoryType::Director(), Utils::parseJSON(root));
  } else {
    LOG_INFO << "No root.json in local storage, defaulting will accept the first root.json provided";
    root_ = Uptane::Root(Uptane::Root::Policy::kAcceptAll);
  }
}

bool AktualizrSecondaryCommon::uptaneInitialize() {
  if (keys_.generateUptaneKeyPair().size() == 0) {
    LOG_ERROR << "Failed to generate uptane key pair";
    return false;
  }

  // from uptane/initialize.cc but we only take care of our own serial/hwid
  EcuSerials ecu_serials;

  if (storage_->loadEcuSerials(&ecu_serials)) {
    ecu_serial_ = ecu_serials[0].first;
    hardware_id_ = ecu_serials[0].second;

    return true;
  }

  std::string ecu_serial_local = config_.uptane.ecu_serial;
  if (ecu_serial_local.empty()) {
    ecu_serial_local = keys_.UptanePublicKey().KeyId();
  }

  std::string ecu_hardware_id = config_.uptane.ecu_hardware_id;
  if (ecu_hardware_id.empty()) {
    ecu_hardware_id = Utils::getHostname();
    if (ecu_hardware_id == "") {
      return false;
    }
  }

  ecu_serials.emplace_back(Uptane::EcuSerial(ecu_serial_local), Uptane::HardwareIdentifier(ecu_hardware_id));
  storage_->storeEcuSerials(ecu_serials);
  ecu_serial_ = ecu_serials[0].first;
  hardware_id_ = ecu_serials[0].second;

  storage_->importInstalledVersions(config_.import.base_path);

  return true;
}
