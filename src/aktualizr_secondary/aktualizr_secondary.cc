#include "aktualizr_secondary.h"

#include <sys/types.h>
#include <future>
#include <memory>

#include "logging/logging.h"
#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif
#include "socket_activation/socket_activation.h"
#include "utilities/utils.h"

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig& config,
                                       const std::shared_ptr<INvStorage>& storage)
    : AktualizrSecondaryCommon(config, storage), conn_(config.network.port) {
  // note: we don't use TlsConfig here and supply the default to
  // KeyManagerConf. Maybe we should figure a cleaner way to do that
  // (split KeyManager?)
  if (!uptaneInitialize()) {
    LOG_ERROR << "Failed to initialize";
    return;
  }
}

void AktualizrSecondary::run() {
  // listen for messages
  std::shared_ptr<SecondaryPacket> pkt;
  while (conn_.in_channel_ >> pkt) {
    std::unique_lock<std::mutex> lock(primaries_mutex);

    sockaddr_storage& peer_addr = pkt->peer_addr;
    auto peer_port = primaries_map.find(peer_addr);
    if (peer_port == primaries_map.end()) {
      continue;
    }

    Utils::setSocketPort(&peer_addr, htons(peer_port->second));

    std::unique_ptr<SecondaryMessage> out_msg;
    switch (pkt->msg->mes_type) {
      case kSecondaryMesPublicKeyReqTag: {
        auto ret = getPublicKeyResp();

        auto* out_msg_pkey = new SecondaryPublicKeyResp;
        out_msg_pkey->type = ret.first;
        out_msg_pkey->key = ret.second;
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_pkey};
        break;
      }
      case kSecondaryMesManifestReqTag: {
        auto ret = getManifestResp();
        auto* out_msg_man = new SecondaryManifestResp;
        out_msg_man->format = kSerializationJson;
        out_msg_man->manifest = Utils::jsonToStr(ret);
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_man};
        break;
      }
      case kSecondaryMesPutMetaReqTag: {
        Uptane::MetaPack meta_pack;

        auto* in_putmeta = dynamic_cast<SecondaryPutMetaReq*>(&(*pkt->msg));
        auto* out_msg_putmeta = new SecondaryPutMetaResp;
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_putmeta};

        if (!in_putmeta->image_meta_present) {
          LOG_ERROR << "Full verification secondary, image meta is expected";
          out_msg_putmeta->result = false;
          break;
        }
        if (in_putmeta->director_targets_format != kSerializationJson ||
            in_putmeta->image_targets_format != kSerializationJson ||
            in_putmeta->image_timestamp_format != kSerializationJson ||
            in_putmeta->image_snapshot_format != kSerializationJson) {
          LOG_ERROR << "Only JSON metadata is supported";
          out_msg_putmeta->result = false;
          break;
        }

        meta_pack.director_targets = Uptane::Targets(Utils::parseJSON(in_putmeta->director_targets));
        meta_pack.image_targets = Uptane::Targets(Utils::parseJSON(in_putmeta->image_targets));
        meta_pack.image_timestamp = Uptane::TimestampMeta(Utils::parseJSON(in_putmeta->image_timestamp));
        meta_pack.image_snapshot = Uptane::Snapshot(Utils::parseJSON(in_putmeta->image_snapshot));
        out_msg_putmeta->result = putMetadataResp(meta_pack);
        break;
      }
      case kSecondaryMesRootVersionReqTag: {
        auto* in_msg_rootversion = dynamic_cast<SecondaryRootVersionReq*>(&(*pkt->msg));
        auto* out_msg_rootversion = new SecondaryRootVersionResp;
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_rootversion};

        out_msg_rootversion->version = getRootVersionResp(in_msg_rootversion->director);
        break;
      }
      case kSecondaryMesPutRootReqTag: {
        auto* in_msg_putroot = dynamic_cast<SecondaryPutRootReq*>(&(*pkt->msg));
        auto* out_msg_putroot = new SecondaryPutRootResp;
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_putroot};

        if (in_msg_putroot->root_format != kSerializationJson) {
          LOG_ERROR << "Only JSON metadata is supported";
          out_msg_putroot->result = false;
          break;
        }

        out_msg_putroot->result = putRootResp(
            Uptane::Root(((in_msg_putroot->director) ? "director" : "repo"), Utils::parseJSON(in_msg_putroot->root)),
            in_msg_putroot->director);

        break;
      }
      case kSecondaryMesSendFirmwareReqTag: {
        auto* in_msg_fw = dynamic_cast<SecondarySendFirmwareReq*>(&(*pkt->msg));
        auto* out_msg_fw = new SecondarySendFirmwareResp;
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_fw};

        out_msg_fw->result = sendFirmwareResp(in_msg_fw->firmware);
        break;
      }
      default:
        LOG_ERROR << "Unexpected message tag: " << pkt->msg->mes_type;
        continue;
    }

    std::shared_ptr<SecondaryPacket> out_pkt{new SecondaryPacket{peer_addr, std::move(out_msg)}};
    conn_.out_channel_ << out_pkt;
  }
}

void AktualizrSecondary::stop() { conn_.stop(); }

std::string AktualizrSecondary::getSerialResp() const { return ecu_serial_; }

std::string AktualizrSecondary::getHwIdResp() const { return hardware_id_; }

std::pair<KeyType, std::string> AktualizrSecondary::getPublicKeyResp() const {
  return std::make_pair(config_.uptane.key_type, keys_.getUptanePublicKey());
}

Json::Value AktualizrSecondary::getManifestResp() const {
  Json::Value manifest = pacman->getManifest(getSerialResp());

  return keys_.signTuf(manifest);
}

bool AktualizrSecondary::putMetadataResp(const Uptane::MetaPack& meta_pack) {
  Uptane::TimeStamp now(Uptane::TimeStamp::Now());
  detected_attack_.clear();

  root_ = Uptane::Root(now, "director", meta_pack.director_root.original(), root_);
  Uptane::Targets targets(now, "director", meta_pack.director_targets.original(), root_);
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
  storage_->storeMetadata(meta_pack);
  return true;
}

int32_t AktualizrSecondary::getRootVersionResp(bool director) const {
  Uptane::MetaPack metapack;
  if (!storage_->loadMetadata(&metapack)) {
    LOG_ERROR << "Could not load metadata";
    return -1;
  }

  if (director) {
    return metapack.director_root.version();
  }
  return metapack.image_root.version();
}

bool AktualizrSecondary::putRootResp(Uptane::Root root, bool director) {
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

    if (res_code != data::UpdateResultCode::OK) {
      LOG_ERROR << "Could not pull from OSTree (" << res_code << "): " << message;
      return false;
    }
#else
    LOG_ERROR << "Could not pull from OSTree. Aktualizr was built without OSTree support!";
    return false;
#endif
  }

  std::tie(res_code, message) = pacman->install(*target_);
  if (res_code != data::UpdateResultCode::OK) {
    LOG_ERROR << "Could not install target (" << res_code << "): " << message;
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
