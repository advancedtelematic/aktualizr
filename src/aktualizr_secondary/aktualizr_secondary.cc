#include "aktualizr_secondary.h"

#include <sys/types.h>
#include <future>
#include <memory>

#include "logging/logging.h"
#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif
#include "socket_activation/socket_activation.h"
#include "socket_server.h"
#include "uptane/secondaryfactory.h"
#include "utilities/utils.h"

class SecondaryAdapter : public Uptane::SecondaryInterface {
 public:
  SecondaryAdapter(const Uptane::SecondaryConfig& sconfig_in, AktualizrSecondary& sec)
      : SecondaryInterface(sconfig_in), secondary(sec) {}
  ~SecondaryAdapter() override = default;

  Uptane::EcuSerial getSerial() override { return secondary.getSerialResp(); }
  Uptane::HardwareIdentifier getHwId() override { return secondary.getHwIdResp(); }
  PublicKey getPublicKey() override { return secondary.getPublicKeyResp(); }
  Json::Value getManifest() override { return secondary.getManifestResp(); }
  bool putMetadata(const Uptane::RawMetaPack& meta_pack) override { return secondary.putMetadataResp(meta_pack); }
  int32_t getRootVersion(bool director) override { return secondary.getRootVersionResp(director); }
  bool putRoot(const std::string& root, bool director) override { return secondary.putRootResp(root, director); }
  bool sendFirmware(const std::string& data) override { return secondary.sendFirmwareResp(data); }

 private:
  AktualizrSecondary& secondary;
};

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig& config,
                                       const std::shared_ptr<INvStorage>& storage)
    : AktualizrSecondaryCommon(config, storage),
      socket_server_(std_::make_unique<SecondaryAdapter>(Uptane::SecondaryConfig(), *this),
                     SocketFromSystemdOrPort(config.network.port)) {
  // note: we don't use TlsConfig here and supply the default to
  // KeyManagerConf. Maybe we should figure a cleaner way to do that
  // (split KeyManager?)
  if (!uptaneInitialize()) {
    LOG_ERROR << "Failed to initialize";
    return;
  }
}

void AktualizrSecondary::run() { socket_server_.Run(); }

void AktualizrSecondary::stop() { /* TODO? */
}

Uptane::EcuSerial AktualizrSecondary::getSerialResp() const { return ecu_serial_; }

Uptane::HardwareIdentifier AktualizrSecondary::getHwIdResp() const { return hardware_id_; }

PublicKey AktualizrSecondary::getPublicKeyResp() const { return keys_.UptanePublicKey(); }

Json::Value AktualizrSecondary::getManifestResp() const {
  Json::Value manifest = pacman->getManifest(getSerialResp());

  return keys_.signTuf(manifest);
}

bool AktualizrSecondary::putMetadataResp(const Uptane::RawMetaPack& meta_pack) {
  Uptane::TimeStamp now(Uptane::TimeStamp::Now());
  detected_attack_.clear();

  // TODO: proper partial verification
  root_ = Uptane::Root(Uptane::RepositoryType::Director, Utils::parseJSON(meta_pack.director_root), root_);
  Uptane::Targets targets(Uptane::RepositoryType::Director, Utils::parseJSON(meta_pack.director_targets), root_);
  if (meta_targets_.version() > targets.version()) {
    detected_attack_ = "Rollback attack detected";
    return true;
  }
  meta_targets_ = targets;
  std::vector<Uptane::Target>::const_iterator it;
  bool target_found = false;
  for (it = meta_targets_.targets.begin(); it != meta_targets_.targets.end(); ++it) {
    if (it->IsForSecondary(getSerialResp())) {
      if (target_found) {
        detected_attack_ = "Duplicate entry for this ECU";
        break;
      }
      target_found = true;
      target_ = std_::make_unique<Uptane::Target>(*it);
    }
  }
  storage_->storeRole(meta_pack.director_root, Uptane::RepositoryType::Director, Uptane::Role::Root(),
                      Uptane::Version(root_.version()));
  storage_->storeRole(meta_pack.director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets(),
                      Uptane::Version(meta_targets_.version()));

  return true;
}

int32_t AktualizrSecondary::getRootVersionResp(bool director) const {
  std::string root_meta;
  if (!storage_->loadRole(&root_meta, (director) ? Uptane::RepositoryType::Director : Uptane::RepositoryType::Images,
                          Uptane::Role::Root())) {
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

bool AktualizrSecondary::sendFirmwareResp(const std::string& firmware) {
  if (target_ == nullptr) {
    LOG_ERROR << "No valid installation target found";
    return false;
  }

  std::string treehub_server;
  try {
    std::string ca, cert, pkey, server_url;
    extractCredentialsArchive(firmware, &ca, &cert, &pkey, &treehub_server);
    keys_.loadKeys(&ca, &cert, &pkey);
    boost::trim(server_url);
    treehub_server = server_url;
  } catch (std::runtime_error& exc) {
    LOG_ERROR << exc.what();

    return false;
  }

  data::UpdateResultCode res_code;
  std::string message;

  if (target_->format().empty() || target_->format() == "OSTREE") {
#ifdef BUILD_OSTREE
    std::tie(res_code, message) =
        OstreeManager::pull(config_.pacman.sysroot, treehub_server, keys_, target_->sha256Hash());

    if (res_code != data::UpdateResultCode::kOk) {
      LOG_ERROR << "Could not pull from OSTree (" << static_cast<int>(res_code) << "): " << message;
      return false;
    }
#else
    LOG_ERROR << "Could not pull from OSTree. Aktualizr was built without OSTree support!";
    return false;
#endif
  }

  std::tie(res_code, message) = pacman->install(*target_);
  if (res_code != data::UpdateResultCode::kOk) {
    LOG_ERROR << "Could not install target (" << static_cast<int>(res_code) << "): " << message;
    return false;
  }

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
    *treehub_server = Utils::readFileFromArchive(as, "server.url");
  }
}
