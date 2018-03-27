#include "ipuptanesecondary.h"

namespace Uptane {

bool IpUptaneSecondary::sendRecv(std::unique_ptr<SecondaryMessage> mes, std::shared_ptr<SecondaryPacket>& resp,
                                 std::chrono::milliseconds to) {
  pending_messages.clear();
  std::shared_ptr<SecondaryPacket> pkt{new SecondaryPacket{sconfig.ip_addr, std::move(mes)}};
  connection->send(pkt);

  pending_messages.setTimeout(to);

  return pending_messages >> resp;
}

std::pair<KeyType, std::string> IpUptaneSecondary::getPublicKey() {
  std::shared_ptr<SecondaryPacket> resp;

  if (!sendRecv(std::unique_ptr<SecondaryPublicKeyReq>(new SecondaryPublicKeyReq{}), resp) ||
      (resp->msg->mes_type != kSecondaryMesPublicKeyRespTag)) {
    return std::make_pair(kUnknownKey, "");
  }

  SecondaryPublicKeyResp& pkey_resp = dynamic_cast<SecondaryPublicKeyResp&>(*resp->msg);

  return std::make_pair(pkey_resp.type, pkey_resp.key);
}

bool IpUptaneSecondary::putMetadata(const MetaPack& meta_pack) {
  std::shared_ptr<SecondaryPacket> resp;
  std::unique_ptr<SecondaryPutMetaReq> req{new SecondaryPutMetaReq()};

  req->image_meta_present = true;
  req->director_targets_format = kSerializationJson;
  req->director_targets = Utils::jsonToStr(meta_pack.director_targets.original());
  req->image_snapshot_format = kSerializationJson;
  req->image_snapshot = Utils::jsonToStr(meta_pack.image_snapshot.original());
  req->image_timestamp_format = kSerializationJson;
  req->image_timestamp = Utils::jsonToStr(meta_pack.image_timestamp.original());
  req->image_targets_format = kSerializationJson;
  req->image_targets = Utils::jsonToStr(meta_pack.image_targets.original());

  return sendRecv(std::move(req), resp) && (resp->msg->mes_type != kSecondaryMesPutMetaRespTag) &&
         dynamic_cast<SecondaryPutMetaResp&>(*resp->msg).result;
}

int32_t IpUptaneSecondary::getRootVersion(const bool director) {
  std::unique_ptr<SecondaryRootVersionReq> req{new SecondaryRootVersionReq()};
  req->director = director;

  std::shared_ptr<SecondaryPacket> resp;

  if (!sendRecv(std::move(req), resp) || (resp->msg->mes_type != kSecondaryMesRootVersionRespTag)) return -1;

  return dynamic_cast<SecondaryRootVersionResp&>(*resp->msg).version;
}

bool IpUptaneSecondary::putRoot(Uptane::Root root, const bool director) {
  std::shared_ptr<SecondaryPacket> resp;

  std::unique_ptr<SecondaryPutRootReq> req{new SecondaryPutRootReq()};
  req->root_format = kSerializationJson;
  req->root = Utils::jsonToStr(root.toJson());
  req->director = director;

  return sendRecv(std::move(req), resp) && (resp->msg->mes_type != kSecondaryMesPutRootRespTag) &&
         dynamic_cast<SecondaryPutRootResp&>(*resp->msg).result;
}

bool IpUptaneSecondary::sendFirmware(const std::string& data) {
  std::shared_ptr<SecondaryPacket> resp;
  std::unique_ptr<SecondarySendFirmwareReq> req{new SecondarySendFirmwareReq()};
  req->firmware = data;

  return sendRecv(std::move(req), resp) && (resp->msg->mes_type != kSecondaryMesSendFirmwareRespTag) &&
         dynamic_cast<SecondarySendFirmwareResp&>(*resp->msg).result;
}

Json::Value IpUptaneSecondary::getManifest() {
  std::shared_ptr<SecondaryPacket> resp;

  if (!sendRecv(std::unique_ptr<SecondaryManifestReq>(new SecondaryManifestReq{}), resp) ||
      (resp->msg->mes_type != kSecondaryMesManifestRespTag))
    return Json::Value();

  SecondaryManifestResp& man_resp = dynamic_cast<SecondaryManifestResp&>(*resp->msg);

  if (man_resp.format != kSerializationJson) return Json::Value();

  return Utils::parseJSON(man_resp.manifest);
}
}  // namespace Uptane
