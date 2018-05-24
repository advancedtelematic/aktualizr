#include "aktualizr_secondary_common.h"
#include "package_manager/packagemanagerfactory.h"
#include "utilities/utils.h"

AktualizrSecondaryCommon::AktualizrSecondaryCommon(const AktualizrSecondaryConfig &config,
                                                   std::shared_ptr<INvStorage> storage)
    : config_(config),
      storage_(std::move(storage)),
      keys_(storage_, config.keymanagerConfig()),
      hardware_id_(Uptane::HardwareIdentifier::Unknown()) {
  pacman = PackageManagerFactory::makePackageManager(config_.pacman, storage_);
}

bool AktualizrSecondaryCommon::uptaneInitialize() {
  if (keys_.generateUptaneKeyPair().size() == 0) {
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

  ecu_serials.emplace_back(ecu_serial_local, Uptane::HardwareIdentifier(ecu_hardware_id));
  storage_->storeEcuSerials(ecu_serials);
  ecu_serial_ = ecu_serials[0].first;
  hardware_id_ = ecu_serials[0].second;

  return true;
}
