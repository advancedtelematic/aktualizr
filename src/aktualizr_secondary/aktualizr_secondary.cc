#include "aktualizr_secondary.h"

#include <sys/types.h>
#include <memory>

#include "logging/logging.h"
#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif
#include "socket_server.h"
#include "utilities/utils.h"

class SecondaryAdapter : public Uptane::SecondaryInterface {
 public:
  SecondaryAdapter(AktualizrSecondary& sec) : secondary(sec) {}
  ~SecondaryAdapter() override = default;

  Uptane::EcuSerial getSerial() override { return secondary.getSerialResp(); }
  Uptane::HardwareIdentifier getHwId() override { return secondary.getHwIdResp(); }
  PublicKey getPublicKey() override { return secondary.getPublicKeyResp(); }
  Json::Value getManifest() override { return secondary.getManifestResp(); }
  bool putMetadata(const Uptane::RawMetaPack& meta_pack) override {
    return secondary.putMetadataResp(Metadata(meta_pack));
  }
  int32_t getRootVersion(bool director) override { return secondary.getRootVersionResp(director); }
  bool putRoot(const std::string& root, bool director) override { return secondary.putRootResp(root, director); }
  bool sendFirmware(const std::shared_ptr<std::string>& data) override {
    return secondary.AktualizrSecondary::sendFirmwareResp(data);
  }
  data::ResultCode::Numeric install(const std::string& target_name) override {
    return secondary.AktualizrSecondary::installResp(target_name);
  }

 private:
  AktualizrSecondary& secondary;
};

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig& config,
                                       const std::shared_ptr<INvStorage>& storage)
    : AktualizrSecondaryCommon(config, storage),
      socket_server_(std_::make_unique<SecondaryAdapter>(*this), SocketFromPort(config.network.port)) {
  // note: we don't use TlsConfig here and supply the default to
  // KeyManagerConf. Maybe we should figure a cleaner way to do that
  // (split KeyManager?)
  if (!uptaneInitialize()) {
    LOG_ERROR << "Failed to initialize";
    return;
  }

  if (rebootDetected()) {
    LOG_INFO << "Reboot has been detected, applying the new ostree revision: " << pending_target_.sha256Hash();
    // TODO: refactor this
    std::vector<Uptane::Target> installed_versions;
    boost::optional<Uptane::Target> pending_target;
    storage->loadInstalledVersions(ecu_serial_.ToString(), nullptr, &pending_target);

    if (!!pending_target) {
      data::InstallationResult install_res = pacman->finalizeInstall(*pending_target);
      storage->saveEcuInstallationResult(ecu_serial_, install_res);
      if (install_res.success) {
        storage->saveInstalledVersion(ecu_serial_.ToString(), *pending_target, InstalledVersionUpdateMode::kCurrent);
      } else {
        // finalize failed
        // unset pending flag so that the rest of the uptane process can
        // go forward again
        storage->saveInstalledVersion(ecu_serial_.ToString(), *pending_target, InstalledVersionUpdateMode::kNone);
        director_repo_.dropTargets(*storage);
      }
    }
    pacman->rebootFlagClear();
  }
}

void AktualizrSecondary::run() {
  connectToPrimary();
  socket_server_.Run();
}

void AktualizrSecondary::stop() { /* TODO? */
}

Uptane::EcuSerial AktualizrSecondary::getSerialResp() const { return ecu_serial_; }

Uptane::HardwareIdentifier AktualizrSecondary::getHwIdResp() const { return hardware_id_; }

PublicKey AktualizrSecondary::getPublicKeyResp() const { return keys_.UptanePublicKey(); }

Json::Value AktualizrSecondary::getManifestResp() const {
  Json::Value manifest = pacman->getManifest(getSerialResp());

  return keys_.signTuf(manifest);
}

int32_t AktualizrSecondary::getRootVersionResp(bool director) const {
  std::string root_meta;
  if (!storage_->loadLatestRoot(&root_meta,
                                (director) ? Uptane::RepositoryType::Director() : Uptane::RepositoryType::Image())) {
    LOG_ERROR << "Could not load root metadata";
    return -1;
  }

  return Uptane::extractVersionUntrusted(root_meta);
}

bool AktualizrSecondary::putRootResp(const std::string& root, bool director) {
  (void)root;
  (void)director;
  LOG_ERROR << "putRootResp is not implemented yet";
  return false;
}

bool AktualizrSecondary::putMetadataResp(const Metadata& metadata) { return doFullVerification(metadata); }

bool AktualizrSecondary::sendFirmwareResp(const std::shared_ptr<std::string>& firmware) {
  if (!pending_target_.IsValid()) {
    LOG_ERROR << "No any pending target to receive update data/image for";
    return false;
  }

  std::unique_ptr<Downloader> downloader;
  // TBD: this stuff has to be refactored and improved
  if (pending_target_.IsOstree()) {
    downloader = std_::make_unique<OstreeDirectDownloader>(config_.pacman.sysroot, keys_);
  } else {
    downloader = std_::make_unique<FakeDownloader>();
  }

  if (!downloader->download(pending_target_, *firmware)) {
    LOG_ERROR << "Failed to pull/store an update data";
    pending_target_ = Uptane::Target::Unknown();
    return false;
  }

  return true;
}

data::ResultCode::Numeric AktualizrSecondary::installResp(const std::string& target_name) {
  if (!pending_target_.IsValid()) {
    LOG_ERROR << "No any pending target to receive update data/image for";
    return data::ResultCode::Numeric::kInternalError;
  }

  if (pending_target_.filename() != target_name) {
    LOG_ERROR << "Targte name to install and the pending target name does not match";
    return data::ResultCode::Numeric::kInternalError;
  }

  auto install_result = pacman->install(pending_target_);

  switch (install_result.result_code.num_code) {
    case data::ResultCode::Numeric::kOk: {
      storage_->saveInstalledVersion(ecu_serial_.ToString(), pending_target_, InstalledVersionUpdateMode::kCurrent);
      pending_target_ = Uptane::Target::Unknown();
      break;
    }
    case data::ResultCode::Numeric::kNeedCompletion: {
      storage_->saveInstalledVersion(ecu_serial_.ToString(), pending_target_, InstalledVersionUpdateMode::kPending);
      break;
    }
    default: {}
  }

  LOG_INFO << "Target has been successfully installed: " << target_name;
  return install_result.result_code.num_code;
}

void AktualizrSecondary::connectToPrimary() {
  Socket socket(config_.network.primary_ip, config_.network.primary_port);

  if (socket.bind(config_.network.port) != 0) {
    LOG_ERROR << "Failed to bind a connection socket to the secondary's port";
    return;
  }

  if (socket.connect() == 0) {
    LOG_INFO << "Connected to Primary, sending info about this secondary...";
    socket_server_.HandleOneConnection(socket.getFD());
  } else {
    LOG_INFO << "Failed to connect to Primary";
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

  auto targetsForThisEcu = director_repo_.getTargets(getSerial(), getHardwareID());

  if (targetsForThisEcu.size() != 1) {
    LOG_ERROR << "Invalid number of targets (should be 1): " << targetsForThisEcu.size();
    return false;
  }

  pending_target_ = targetsForThisEcu[0];

  return true;
}

bool OstreeDirectDownloader::download(const Uptane::Target& target, const std::string& data) {
  std::string treehub_server;
  bool download_result = false;

  try {
    std::string ca, cert, pkey, server_url;
    extractCredentialsArchive(data, &ca, &cert, &pkey, &server_url);
    _keyMngr.loadKeys(&pkey, &cert, &ca);
    boost::trim(server_url);
    treehub_server = server_url;
  } catch (std::runtime_error& exc) {
    LOG_ERROR << exc.what();
    return false;
  }

  auto install_res = OstreeManager::pull(_sysrootPath, treehub_server, _keyMngr, target);

  switch (install_res.result_code.num_code) {
    case data::ResultCode::Numeric::kOk: {
      LOG_INFO << "The target revision has been successfully downloaded: " << target.sha256Hash();
      download_result = true;
      break;
    }
    case data::ResultCode::Numeric::kAlreadyProcessed: {
      LOG_INFO << "The target revision is already present on the local ostree repo: " << target.sha256Hash();
      download_result = true;
      break;
    }
    default: {
      LOG_ERROR << "Failed to download the target revision: " << target.sha256Hash() << " ( "
                << install_res.result_code.toString() << " ): " << install_res.description;
    }
  }

  return download_result;
}

void OstreeDirectDownloader::extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert,
                                                       std::string* pkey, std::string* treehub_server) {
  ::extractCredentialsArchive(archive, ca, cert, pkey, treehub_server);
}

void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert, std::string* pkey,
                               std::string* treehub_server) {
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

bool FakeDownloader::download(const Uptane::Target& target, const std::string& data) {
  auto target_hashes = target.hashes();
  if (target_hashes.size() == 0) {
    LOG_ERROR << "No hash found in the target metadata: " << target.filename();
    return false;
  }

  try {
    auto received_image_data_hash = Uptane::Hash::generate(target_hashes[0].type(), data);

    if (!target.MatchHash(received_image_data_hash)) {
      LOG_ERROR << "The received image data hash doesn't match the hash specified in the target metadata,"
                   " hash type: "
                << target_hashes[0].TypeString();
      return false;
    }

  } catch (const std::exception& exc) {
    LOG_ERROR << "Failed to generate a hash of the received image data: " << exc.what();
    return false;
  }

  return true;
}
