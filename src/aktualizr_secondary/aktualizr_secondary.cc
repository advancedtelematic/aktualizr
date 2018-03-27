#include "aktualizr_secondary.h"

#include <sys/types.h>
#include <future>

#include "invstorage.h"
#include "logging.h"
#include "socket_activation.h"
#include "utils.h"

AktualizrSecondary::AktualizrSecondary(const AktualizrSecondaryConfig& config,
                                       const boost::shared_ptr<INvStorage>& storage)
    : config_(config), conn_(config.network.port), storage_(storage), keys_(storage_, config.keymanagerConfig()) {
  pacman = PackageManagerFactory::makePackageManager(config_.pacman, storage_);
  // note: we don't use TlsConfig here and supply the default to
  // KeyManagerConf. Maybe we should figure a cleaner way to do that
  // (split KeyManager?)
  if (!uptaneInitialize()) {
    LOG_ERROR << "Failed to initialize";
    return;
  }
}

// Initialize the parts needed for a secondary
// - ECU keys
// - serial id
bool AktualizrSecondary::uptaneInitialize() {
  if (keys_.generateUptaneKeyPair().size() == 0) {
    return false;
  }

  // from uptane/initialize.cc but we only take care of our own serial/hwid
  std::vector<std::pair<std::string, std::string> > ecu_serials;

  if (storage_->loadEcuSerials(&ecu_serials)) {
    ecu_serial_ = ecu_serials[0].first;
    hardware_id_ = ecu_serials[0].second;
    return true;
  }

  std::string ecu_serial_local = config_.uptane.ecu_serial;
  if (ecu_serial_local.empty()) {
    ecu_serial_local = Crypto::getKeyId(keys_.getUptanePublicKey());
  }

  std::string ecu_hardware_id = config_.uptane.ecu_hardware_id;
  if (ecu_hardware_id.empty()) {
    ecu_hardware_id = Utils::getHostname();
    if (ecu_hardware_id == "") return false;
  }

  ecu_serials.push_back(std::pair<std::string, std::string>(ecu_serial_local, ecu_hardware_id));
  storage_->storeEcuSerials(ecu_serials);
  ecu_serial_ = ecu_serials[0].first;
  hardware_id_ = ecu_serials[0].second;

  return true;
}

void AktualizrSecondary::run() {
  // listen for messages
  std::shared_ptr<SecondaryPacket> pkt;
  while (conn_.in_channel_ >> pkt) {
    std::unique_lock<std::mutex> lock(primaries_mutex);

    sockaddr_storage& peer_addr = pkt->peer_addr;
    auto peer_port = primaries_map.find(peer_addr);
    if (peer_port == primaries_map.end()) continue;

    Utils::setSocketPort(&peer_addr, htons(peer_port->second));

    std::unique_ptr<SecondaryMessage> out_msg;
    switch (pkt->msg->mes_type) {
      case kSecondaryMesPublicKeyReqTag: {
        auto ret = getPublicKeyResp();

        SecondaryPublicKeyResp* out_msg_pkey = new SecondaryPublicKeyResp;
        out_msg_pkey->type = ret.first;
        out_msg_pkey->key = ret.second;
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_pkey};
        break;
      }
      case kSecondaryMesManifestReqTag: {
        auto ret = getManifestResp();
        SecondaryManifestResp* out_msg_man = new SecondaryManifestResp;
        out_msg_man->format = kSerializationJson;
        out_msg_man->manifest = Utils::jsonToStr(ret);
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_man};
        break;
      }
      case kSecondaryMesPutMetaReqTag: {
        Uptane::MetaPack meta_pack;

        SecondaryPutMetaReq* in_putmeta = dynamic_cast<SecondaryPutMetaReq*>(&(*pkt->msg));
        SecondaryPutMetaResp* out_msg_putmeta = new SecondaryPutMetaResp;
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
        SecondaryRootVersionReq* in_msg_rootversion = dynamic_cast<SecondaryRootVersionReq*>(&(*pkt->msg));
        SecondaryRootVersionResp* out_msg_rootversion = new SecondaryRootVersionResp;
        out_msg = std::unique_ptr<SecondaryMessage>{out_msg_rootversion};

        out_msg_rootversion->version = getRootVersionResp(in_msg_rootversion->director);
        break;
      }
      case kSecondaryMesPutRootReqTag: {
        SecondaryPutRootReq* in_msg_putroot = dynamic_cast<SecondaryPutRootReq*>(&(*pkt->msg));
        SecondaryPutRootResp* out_msg_putroot = new SecondaryPutRootResp;
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
        SecondarySendFirmwareReq* in_msg_fw = dynamic_cast<SecondarySendFirmwareReq*>(&(*pkt->msg));
        SecondarySendFirmwareResp* out_msg_fw = new SecondarySendFirmwareResp;
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

Json::Value AktualizrSecondary::getManifestResp() const { return pacman->getManifest(getSerialResp()); }

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
      target = boost::movelib::make_unique<Uptane::Target>(*it);
    }
  }
  return true;
}
#ifdef BUILD_OSTREE
bool AktualizrSecondary::sendFirmwareOstreResp(const std::string& cert, const std::string& pkey,
                                               const std::string& ca) {
  KeyManagerConfig keysconfig;  // by default keysource is kFile, for now it is ok.
  KeyManager keys(storage_, keysconfig);
  keys.loadKeys(&pkey, &cert, &ca);
  OstreeManager::pull(config_.pacman, keys, target->sha256Hash());
  return (pacman->install(*target).first == data::UpdateResultCode::OK);
}
#endif

int32_t AktualizrSecondary::getRootVersionResp(bool director) const {
  (void)director;
  LOG_ERROR << "getRootVersionResp is not implemented yet";
  return -1;
}

bool AktualizrSecondary::putRootResp(Uptane::Root root, bool director) {
  (void)root;
  (void)director;
  LOG_ERROR << "putRootResp is not implemented yet";
  return false;
}

bool AktualizrSecondary::sendFirmwareResp(const std::string& firmware) {
  (void)firmware;
  LOG_ERROR << "sendFirmwareResp is not implemented yet";
  return false;
}
