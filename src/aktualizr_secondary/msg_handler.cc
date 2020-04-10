#include "msg_handler.h"

#include "logging/logging.h"

void MsgDispatcher::registerHandler(AKIpUptaneMes_PR msg_id, Handler handler) {
  handler_map_[msg_id] = std::move(handler);
}

MsgHandler::ReturnCode MsgDispatcher::handleMsg(const Asn1Message::Ptr& in_msg, Asn1Message::Ptr& out_msg) {
  LOG_INFO << "Got a request message from Primary: " << in_msg->toStr();

  auto find_res_it = handler_map_.find(in_msg->present());
  if (find_res_it == handler_map_.end()) {
    return MsgHandler::kUnkownMsg;
  }
  LOG_INFO << "Found a handler for the request message, processing it...";
  auto handle_status_code = find_res_it->second(*in_msg, *out_msg);
  LOG_INFO << "Got a response message from a handler: " << out_msg->toStr();

  return handle_status_code;
}
