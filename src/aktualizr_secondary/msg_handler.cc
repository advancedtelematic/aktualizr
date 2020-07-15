#include "msg_handler.h"

#include "logging/logging.h"

void MsgDispatcher::clearHandlers() { handler_map_.clear(); }

void MsgDispatcher::registerHandler(AKIpUptaneMes_PR msg_id, Handler handler) {
  handler_map_[msg_id] = std::move(handler);
}

MsgHandler::ReturnCode MsgDispatcher::handleMsg(const Asn1Message::Ptr& in_msg, Asn1Message::Ptr& out_msg) {
  auto find_res_it = handler_map_.find(in_msg->present());
  if (find_res_it == handler_map_.end()) {
    return MsgHandler::kUnkownMsg;
  }
  LOG_TRACE << "Found a handler for the request, processing it...";
  auto handle_status_code = find_res_it->second(*in_msg, *out_msg);
  LOG_TRACE << "Request handler returned a response: " << out_msg->toStr();

  // Track the last message to help cut down on repetitive logging. Ignore the
  // version messages since they just get in the way.
  if (in_msg->present() != AKIpUptaneMes_PR_versionReq) {
    last_msg_ = in_msg->present();
  }
  return handle_status_code;
}
