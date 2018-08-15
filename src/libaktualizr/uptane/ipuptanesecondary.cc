#include "ipuptanesecondary.h"
#include "asn1/asn1_message.h"
#include "der_encoder.h"
#include "logging/logging.h"

#include <memory>

namespace Uptane {

PublicKey IpUptaneSecondary::getPublicKey() {
  LOG_INFO << "Getting the public key of a secondary";
  Asn1Message::Ptr req(Asn1Message::Empty());

  req->present(AKIpUptaneMes_PR_publicKeyReq);

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_publicKeyResp) {
    LOG_ERROR << "Failed to get public key response message from secondary";
    return PublicKey("", KeyType::kUnknown);
  }
  auto r = resp->publicKeyResp();

  std::string key = ToString(r->key);

  auto type = static_cast<KeyType>(r->type);
  return PublicKey(key, type);
}

bool IpUptaneSecondary::putMetadata(const RawMetaPack& meta_pack) {
  LOG_INFO << "Sending Uptane metadata to the secondary";
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_putMetaReq);

  auto m = req->putMetaReq();
  m->image.present = image_PR_json;
  SetString(&m->image.choice.json.root, meta_pack.image_root);            // NOLINT
  SetString(&m->image.choice.json.targets, meta_pack.image_targets);      // NOLINT
  SetString(&m->image.choice.json.snapshot, meta_pack.image_snapshot);    // NOLINT
  SetString(&m->image.choice.json.timestamp, meta_pack.image_timestamp);  // NOLINT

  m->director.present = director_PR_json;
  SetString(&m->director.choice.json.root, meta_pack.director_root);        // NOLINT
  SetString(&m->director.choice.json.targets, meta_pack.director_targets);  // NOLINT

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_putMetaResp) {
    LOG_ERROR << "Failed to get response to sending manifest to secondary";
    return false;
  }

  auto r = resp->putMetaResp();
  return r->result == AKInstallationResult_success;
}

bool IpUptaneSecondary::sendFirmwareAsync(const std::shared_ptr<std::string>& data) {
  if (!install_future.valid() || install_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    install_future = std::async(std::launch::async, &IpUptaneSecondary::sendFirmware, this, std::ref(data));
    return true;
  }
  return false;
}

bool IpUptaneSecondary::sendFirmware(const std::shared_ptr<std::string>& data) {
  LOG_INFO << "Sending firmware to the secondary";
  *events_channel << std::make_shared<event::InstallStarted>(getSerial());
  Asn1Message::Ptr req(Asn1Message::Empty());
  req->present(AKIpUptaneMes_PR_sendFirmwareReq);

  auto m = req->sendFirmwareReq();
  SetString(&m->firmware, *data);
  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_sendFirmwareResp) {
    LOG_ERROR << "Failed to get response to sending firmware to secondary";
    *events_channel << std::make_shared<event::InstallComplete>(getSerial());
    return false;
  }

  auto r = resp->sendFirmwareResp();
  *events_channel << std::make_shared<event::InstallComplete>(getSerial());
  return r->result == AKInstallationResult_success;
}

Json::Value IpUptaneSecondary::getManifest() {
  LOG_INFO << "Getting the manifest key of a secondary";
  Asn1Message::Ptr req(Asn1Message::Empty());

  req->present(AKIpUptaneMes_PR_manifestReq);

  auto resp = Asn1Rpc(req, getAddr());

  if (resp->present() != AKIpUptaneMes_PR_manifestResp) {
    LOG_ERROR << "Failed to get public key response message from secondary";
    return Json::Value();
  }
  auto r = resp->manifestResp();

  if (r->manifest.present != manifest_PR_json) {
    LOG_ERROR << "Manifest wasn't in json format";
    return Json::Value();
  }
  std::string manifest = ToString(r->manifest.choice.json);  // NOLINT
  return Utils::parseJSON(manifest);
}
}  // namespace Uptane
