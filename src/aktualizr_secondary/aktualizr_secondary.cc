#include "aktualizr_secondary.h"

#include "crypto/keymanager.h"
#include "logging/logging.h"
#include "update_agent.h"
#include "uptane/manifest.h"
#include "utilities/utils.h"

#include <sys/types.h>
#include <memory>

AktualizrSecondary::AktualizrSecondary(AktualizrSecondaryConfig config, std::shared_ptr<INvStorage> storage,
                                       std::shared_ptr<Bootloader> bootloader, std::shared_ptr<KeyManager> key_mngr,
                                       std::shared_ptr<UpdateAgent> update_agent)
    : config_(std::move(config)),
      storage_(std::move(storage)),
      bootloader_(std::move(bootloader)),
      keys_(std::move(key_mngr)),
      update_agent_(std::move(update_agent)) {
  if (!uptaneInitialize()) {
    LOG_ERROR << "Failed to initialize";
    return;
  }

  manifest_issuer_ = std::make_shared<Uptane::ManifestIssuer>(keys_, ecu_serial_);

  if (rebootDetected()) {
    LOG_INFO << "Reboot has been detected, applying the new ostree revision: " << pending_target_.sha256Hash();
    // TODO: refactor this to make it simpler as we don't need to persist/store
    // an installation status of each ECU but store it just for a given secondary ECU
    std::vector<Uptane::Target> installed_versions;
    boost::optional<Uptane::Target> pending_target;
    storage_->loadInstalledVersions(ecu_serial_.ToString(), nullptr, &pending_target);

    if (!!pending_target) {
      data::InstallationResult install_res = update_agent_->applyPendingInstall(*pending_target);
      storage_->saveEcuInstallationResult(ecu_serial_, install_res);
      if (install_res.success) {
        storage_->saveInstalledVersion(ecu_serial_.ToString(), *pending_target, InstalledVersionUpdateMode::kCurrent);
      } else {
        storage_->saveInstalledVersion(ecu_serial_.ToString(), *pending_target, InstalledVersionUpdateMode::kNone);
        director_repo_.dropTargets(*storage_);
      }
    }
    bootloader_->rebootFlagClear();
  }
}

Uptane::EcuSerial AktualizrSecondary::getSerial() const { return ecu_serial_; }

Uptane::HardwareIdentifier AktualizrSecondary::getHwId() const { return hardware_id_; }

PublicKey AktualizrSecondary::getPublicKey() const { return keys_->UptanePublicKey(); }

Uptane::Manifest AktualizrSecondary::getManifest() const {
  Uptane::InstalledImageInfo installed_image_info;
  Uptane::Manifest manifest;
  if (update_agent_->getInstalledImageInfo(installed_image_info)) {
    manifest = manifest_issuer_->assembleAndSignManifest(installed_image_info);
  }

  return manifest;
}

int32_t AktualizrSecondary::getRootVersion(bool director) const {
  std::string root_meta;
  if (!storage_->loadLatestRoot(&root_meta,
                                (director) ? Uptane::RepositoryType::Director() : Uptane::RepositoryType::Image())) {
    LOG_ERROR << "Could not load root metadata";
    return -1;
  }

  return Uptane::extractVersionUntrusted(root_meta);
}

bool AktualizrSecondary::putRoot(const std::string& root, bool director) {
  (void)root;
  (void)director;
  LOG_ERROR << "putRootResp is not implemented yet";
  return false;
}

bool AktualizrSecondary::putMetadata(const Metadata& metadata) { return doFullVerification(metadata); }

bool AktualizrSecondary::sendFirmware(const std::string& firmware) {
  // TODO: how to handle the case when secondary is rebooted after metadata are received
  if (!pending_target_.IsValid()) {
    LOG_ERROR << "No any pending target to receive update data/image for";
    return false;
  }

  if (!update_agent_->download(pending_target_, firmware)) {
    LOG_ERROR << "Failed to pull/store an update data";
    pending_target_ = Uptane::Target::Unknown();
    return false;
  }

  return true;
}

data::ResultCode::Numeric AktualizrSecondary::install(const std::string& target_name) {
  // TODO: how to handle the case when secondary is rebooted after metadata are received
  if (!pending_target_.IsValid()) {
    LOG_ERROR << "No any pending target to receive update data/image for";
    return data::ResultCode::Numeric::kInternalError;
  }

  if (pending_target_.filename() != target_name) {
    LOG_ERROR << "name of the target to install and a name of the pending target do not match";
    return data::ResultCode::Numeric::kInternalError;
  }

  auto install_result = update_agent_->install(pending_target_);

  switch (install_result) {
    case data::ResultCode::Numeric::kOk: {
      storage_->saveInstalledVersion(ecu_serial_.ToString(), pending_target_, InstalledVersionUpdateMode::kCurrent);
      pending_target_ = Uptane::Target::Unknown();
      LOG_INFO << "The target has been successfully installed: " << target_name;
      break;
    }
    case data::ResultCode::Numeric::kNeedCompletion: {
      storage_->saveInstalledVersion(ecu_serial_.ToString(), pending_target_, InstalledVersionUpdateMode::kPending);
      LOG_INFO << "The target has been successfully installed, but a reboot is required to be applied: " << target_name;
      break;
    }
    default: { LOG_INFO << "Failed to install the target: " << target_name; }
  }

  return install_result;
}

bool AktualizrSecondary::doFullVerification(const Metadata& metadata) {
  // 5.4.4.2. Full verification  https://uptane.github.io/uptane-standard/uptane-standard.html#metadata_verification

  // 1. Load and verify the current time or the most recent securely attested time.
  //
  //    We trust the time that the given system/OS/ECU provides, In ECU We Trust :)
  TimeStamp now(TimeStamp::Now());

  // 2. Download and check the Root metadata file from the Director repository, following the procedure in
  // Section 5.4.4.3. DirectorRepository::updateMeta() method implements this verification step, certain steps are
  // missing though. see the method source code for details

  // 3. NOT SUPPORTED: Download and check the Timestamp metadata file from the Director repository, following the
  // procedure in Section 5.4.4.4.
  // 4. NOT SUPPORTED: Download and check the Snapshot metadata file from the Director repository, following the
  // procedure in Section 5.4.4.5.
  //
  // 5. Download and check the Targets metadata file from the Director repository, following the procedure in
  // Section 5.4.4.6. DirectorRepository::updateMeta() method implements this verification step
  //
  // The followin steps of the Director's target metadata verification are missing in DirectorRepository::updateMeta()
  //  6. If checking Targets metadata from the Director repository, verify that there are no delegations.
  //  7. If checking Targets metadata from the Director repository, check that no ECU identifier is represented more
  //  than once.
  if (!director_repo_.updateMeta(*storage_, metadata)) {
    LOG_ERROR << "Failed to update director metadata: " << director_repo_.getLastException().what();
    return false;
  }

  // 6. Download and check the Root metadata file from the Image repository, following the procedure in Section 5.4.4.3.
  // 7. Download and check the Timestamp metadata file from the Image repository, following the procedure in
  // Section 5.4.4.4.
  // 8. Download and check the Snapshot metadata file from the Image repository, following the procedure in
  // Section 5.4.4.5.
  // 9. Download and check the top-level Targets metadata file from the Image repository, following the procedure in
  // Section 5.4.4.6.
  if (!image_repo_.updateMeta(*storage_, metadata)) {
    LOG_ERROR << "Failed to update image metadata: " << image_repo_.getLastException().what();
    return false;
  }

  // 10. Verify that Targets metadata from the Director and Image repositories match.
  if (!director_repo_.matchTargetsWithImageTargets(*(image_repo_.getTargets()))) {
    LOG_ERROR << "Targets metadata from the Director and Image repositories DOES NOT match ";
    return false;
  }

  auto targetsForThisEcu = director_repo_.getTargets(getSerial(), getHwId());

  if (targetsForThisEcu.size() != 1) {
    LOG_ERROR << "Invalid number of targets (should be 1): " << targetsForThisEcu.size();
    return false;
  }

  if (!update_agent_->isTargetSupported(targetsForThisEcu[0])) {
    LOG_ERROR << "The given target type is not supported: " << targetsForThisEcu[0].type();
    return false;
  }

  pending_target_ = targetsForThisEcu[0];

  return true;
}

bool AktualizrSecondary::uptaneInitialize() {
  if (keys_->generateUptaneKeyPair().size() == 0) {
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
    ecu_serial_local = keys_->UptanePublicKey().KeyId();
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

  return true;
}
