#include "msg_dispatcher.h"

#include <functional>
#include <unordered_map>

#include "logging/logging.h"

void MsgDispatcher::registerHandler(AKIpUptaneMes_PR msg_id, Handler handler) {
  handler_map_[msg_id] = std::move(handler);
}

MsgDispatcher::HandleStatusCode MsgDispatcher::handleMsg(const Asn1Message::Ptr& in_msg, Asn1Message::Ptr& out_msg) {
  LOG_TRACE << "Got a request message from Primary: " << in_msg->toStr();

  auto find_res_it = handler_map_.find(in_msg->present());
  if (find_res_it == handler_map_.end()) {
    return MsgDispatcher::HandleStatusCode::kUnkownMsg;
  }
  LOG_TRACE << "Found a handler for the request message, processing it...";
  auto handle_status_code = find_res_it->second(*in_msg, *out_msg);
  LOG_TRACE << "Got a response message from a handler: " << out_msg->toStr();

  return handle_status_code;
}

AktualizrSecondaryMsgDispatcher::AktualizrSecondaryMsgDispatcher(IAktualizrSecondary& secondary)
    : secondary_{secondary} {
  registerHandler(AKIpUptaneMes_PR_getInfoReq, std::bind(&AktualizrSecondaryMsgDispatcher::getInfoHdlr, this,
                                                         std::placeholders::_1, std::placeholders::_2));

  registerHandler(AKIpUptaneMes_PR_manifestReq, std::bind(&AktualizrSecondaryMsgDispatcher::getManifestHdlr, this,
                                                          std::placeholders::_1, std::placeholders::_2));

  registerHandler(AKIpUptaneMes_PR_putMetaReq, std::bind(&AktualizrSecondaryMsgDispatcher::putMetaHdlr, this,
                                                         std::placeholders::_1, std::placeholders::_2));

  registerHandler(AKIpUptaneMes_PR_sendFirmwareReq, std::bind(&AktualizrSecondaryMsgDispatcher::sendFirmwareHdlr, this,
                                                              std::placeholders::_1, std::placeholders::_2));

  registerHandler(AKIpUptaneMes_PR_installReq, std::bind(&AktualizrSecondaryMsgDispatcher::installHdlr, this,
                                                         std::placeholders::_1, std::placeholders::_2));
}

MsgDispatcher::HandleStatusCode AktualizrSecondaryMsgDispatcher::getInfoHdlr(Asn1Message& in_msg,
                                                                             Asn1Message& out_msg) {
  (void)in_msg;

  Uptane::EcuSerial serial = Uptane::EcuSerial::Unknown();
  Uptane::HardwareIdentifier hw_id = Uptane::HardwareIdentifier::Unknown();
  PublicKey pub_key;

  std::tie(serial, hw_id, pub_key) = secondary_.getInfo();
  out_msg.present(AKIpUptaneMes_PR_getInfoResp);

  auto info_resp = out_msg.getInfoResp();

  SetString(&info_resp->ecuSerial, serial.ToString());
  SetString(&info_resp->hwId, hw_id.ToString());
  info_resp->keyType = static_cast<AKIpUptaneKeyType_t>(pub_key.Type());
  SetString(&info_resp->key, pub_key.Value());

  return HandleStatusCode::kOk;
}

MsgDispatcher::HandleStatusCode AktualizrSecondaryMsgDispatcher::getManifestHdlr(Asn1Message& in_msg,
                                                                                 Asn1Message& out_msg) {
  (void)in_msg;

  std::string manifest = Utils::jsonToStr(secondary_.getManifest());
  out_msg.present(AKIpUptaneMes_PR_manifestResp);
  auto manifest_resp = out_msg.manifestResp();
  manifest_resp->manifest.present = manifest_PR_json;
  SetString(&manifest_resp->manifest.choice.json, manifest);  // NOLINT

  LOG_TRACE << "Manifest : \n" << manifest;
  return HandleStatusCode::kOk;
}

MsgDispatcher::HandleStatusCode AktualizrSecondaryMsgDispatcher::putMetaHdlr(Asn1Message& in_msg,
                                                                             Asn1Message& out_msg) {
  auto md = in_msg.putMetaReq();
  Uptane::RawMetaPack meta_pack;

  if (md->director.present == director_PR_json) {
    meta_pack.director_root = ToString(md->director.choice.json.root);        // NOLINT
    meta_pack.director_targets = ToString(md->director.choice.json.targets);  // NOLINT
    LOG_DEBUG << "Received Director repo Root metadata:\n" << meta_pack.director_root;
    LOG_DEBUG << "Received Director repo Targets metadata:\n" << meta_pack.director_targets;
  } else {
    LOG_WARNING << "Director metadata in unknown format:" << md->director.present;
  }

  if (md->image.present == image_PR_json) {
    meta_pack.image_root = ToString(md->image.choice.json.root);            // NOLINT
    meta_pack.image_timestamp = ToString(md->image.choice.json.timestamp);  // NOLINT
    meta_pack.image_snapshot = ToString(md->image.choice.json.snapshot);    // NOLINT
    meta_pack.image_targets = ToString(md->image.choice.json.targets);      // NOLINT
    LOG_DEBUG << "Received Image repo Root metadata:\n" << meta_pack.image_root;
    LOG_DEBUG << "Received Image repo Timestamp metadata:\n" << meta_pack.image_timestamp;
    LOG_DEBUG << "Received Image repo Snapshot metadata:\n" << meta_pack.image_snapshot;
    LOG_DEBUG << "Received Image repo Targets metadata:\n" << meta_pack.image_targets;
  } else {
    LOG_WARNING << "Image repo metadata in unknown format:" << md->image.present;
  }
  bool ok = secondary_.putMetadata(meta_pack);

  out_msg.present(AKIpUptaneMes_PR_putMetaResp).putMetaResp()->result =
      ok ? AKInstallationResult_success : AKInstallationResult_failure;

  return HandleStatusCode::kOk;
}

MsgDispatcher::HandleStatusCode AktualizrSecondaryMsgDispatcher::sendFirmwareHdlr(Asn1Message& in_msg,
                                                                                  Asn1Message& out_msg) {
  auto fw = in_msg.sendFirmwareReq();
  auto send_firmware_result = secondary_.sendFirmware(ToString(in_msg.sendFirmwareReq()->firmware));

  out_msg.present(AKIpUptaneMes_PR_sendFirmwareResp).sendFirmwareResp()->result =
      send_firmware_result ? AKInstallationResult_success : AKInstallationResult_failure;
  ;

  return HandleStatusCode::kOk;
}

MsgDispatcher::HandleStatusCode AktualizrSecondaryMsgDispatcher::installHdlr(Asn1Message& in_msg,
                                                                             Asn1Message& out_msg) {
  auto install_result = secondary_.install(ToString(in_msg.installReq()->hash));
  out_msg.present(AKIpUptaneMes_PR_installResp).installResp()->result =
      static_cast<AKInstallationResultCode_t>(install_result);

  if (data::ResultCode::Numeric::kNeedCompletion == install_result) {
    return HandleStatusCode::kRebootRequired;
  }

  return HandleStatusCode::kOk;
}
