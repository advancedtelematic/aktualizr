#include "aktualizr_secondary.h"

#include <sys/types.h>
#include <memory>

#include "logging/logging.h"
#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif
#include "socket_server.h"
#include "utilities/utils.h"

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig& config,
                                       const std::shared_ptr<INvStorage>& storage,
                                       const std::shared_ptr<KeyManager>& key_mngr,
                                       const std::shared_ptr<PackageManagerInterface>& pacman)
    : config_{config}, storage_{storage}, keys_{key_mngr}, pacman_{pacman} {
  // note: we don't use TlsConfig here and supply the default to
  // KeyManagerConf. Maybe we should figure a cleaner way to do that
  // (split KeyManager?)
  if (!uptaneInitialize()) {
    LOG_ERROR << "Failed to initialize";
    return;
  }
}

Uptane::EcuSerial AktualizrSecondary::getSerial() { return ecu_serial_; }

Uptane::HardwareIdentifier AktualizrSecondary::getHwId() { return hardware_id_; }

PublicKey AktualizrSecondary::getPublicKey() { return keys_->UptanePublicKey(); }

Json::Value AktualizrSecondary::getManifest() {
  Json::Value manifest = pacman_->getManifest(getSerial());

  return keys_->signTuf(manifest);
}

bool AktualizrSecondary::putMetadata(const Uptane::RawMetaPack& meta_pack) {
  return doFullVerification(Metadata(meta_pack));
}

int32_t AktualizrSecondary::getRootVersion(bool director) {
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

bool AktualizrSecondary::sendFirmware(const std::shared_ptr<std::string>& firmware) {
  auto targetsForThisEcu = director_repo_.getTargets(getSerial(), getHwId());

  if (targetsForThisEcu.size() != 1) {
    LOG_ERROR << "Invalid number of targets (should be one): " << targetsForThisEcu.size();
    return false;
  }
  auto target_to_apply = targetsForThisEcu[0];

  std::string treehub_server;
  std::size_t firmware_size = firmware->length();

  if (target_to_apply.IsOstree()) {
    // this is the ostree specific case
    try {
      std::string ca, cert, pkey, server_url;
      extractCredentialsArchive(*firmware, &ca, &cert, &pkey, &server_url);
      keys_->loadKeys(&ca, &cert, &pkey);
      boost::trim(server_url);
      treehub_server = server_url;
      firmware_size = 0;
    } catch (std::runtime_error& exc) {
      LOG_ERROR << exc.what();

      return false;
    }
  }

  data::InstallationResult install_res;

  if (target_to_apply.IsOstree()) {
#ifdef BUILD_OSTREE
    install_res = OstreeManager::pull(config_.pacman.sysroot, treehub_server, *keys_, target_to_apply);

    if (install_res.result_code.num_code != data::ResultCode::Numeric::kOk) {
      LOG_ERROR << "Could not pull from OSTree (" << install_res.result_code.toString()
                << "): " << install_res.description;
      return false;
    }
#else
    LOG_ERROR << "Could not pull from OSTree. Aktualizr was built without OSTree support!";
    return false;
#endif
  } else if (pacman_->name() == "debian") {
    // TODO save debian package here.
    LOG_ERROR << "Installation of debian images is not suppotrted yet.";
    return false;
  }

  if (target_to_apply.length() != firmware_size) {
    LOG_ERROR << "The target image size specified in metadata " << target_to_apply.length()
              << " does not match actual size " << firmware->length();
    return false;
  }

  if (!target_to_apply.IsOstree()) {
    auto target_hashes = target_to_apply.hashes();
    if (target_hashes.size() == 0) {
      LOG_ERROR << "No hash found in the target metadata: " << target_to_apply.filename();
      return false;
    }

    try {
      auto received_image_data_hash = Uptane::Hash::generate(target_hashes[0].type(), *firmware);

      if (!target_to_apply.MatchHash(received_image_data_hash)) {
        LOG_ERROR << "The received image data hash doesn't match the hash specified in the target metadata,"
                     " hash type: "
                  << target_hashes[0].TypeString();
        return false;
      }

    } catch (const std::exception& exc) {
      LOG_ERROR << "Failed to generate a hash of the received image data: " << exc.what();
      return false;
    }
  }

  install_res = pacman_->install(target_to_apply);
  if (install_res.result_code.num_code != data::ResultCode::Numeric::kOk &&
      install_res.result_code.num_code != data::ResultCode::Numeric::kNeedCompletion) {
    LOG_ERROR << "Could not install target (" << install_res.result_code.toString() << "): " << install_res.description;
    return false;
  }

  if (install_res.result_code.num_code == data::ResultCode::Numeric::kNeedCompletion) {
    storage_->saveInstalledVersion(getSerial().ToString(), target_to_apply, InstalledVersionUpdateMode::kPending);
  } else {
    storage_->saveInstalledVersion(getSerial().ToString(), target_to_apply, InstalledVersionUpdateMode::kCurrent);
  }

  // TODO: https://saeljira.it.here.com/browse/OTA-4174
  // - return data::InstallationResult to Primary
  // - add finaliztion support, pacman_->finalizeInstall() must be called at secondary startup if there is pending
  // version

  return true;
}

void AktualizrSecondary::extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert,
                                                   std::string* pkey, std::string* treehub_server) {
  {
    std::stringstream as(archive);
    *ca = Utils::readFileFromArchive(as, "ca.pem");
  }
  {
    std::stringstream as(archive);
    *cert = Utils::readFileFromArchive(as, "client.pem");
  }
  {
    std::stringstream as(archive);
    *pkey = Utils::readFileFromArchive(as, "pkey.pem");
  }
  {
    std::stringstream as(archive);
    *treehub_server = Utils::readFileFromArchive(as, "server.url", true);
  }
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

  auto targetsForThisEcu = director_repo_.getTargets(getSerial(), getHwId());

  if (targetsForThisEcu.size() != 1) {
    LOG_ERROR << "Invalid number of targets (should be 1): " << targetsForThisEcu.size();
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
