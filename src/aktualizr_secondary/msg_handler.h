#ifndef MSG_HANDLER_H
#define MSG_HANDLER_H

#include <memory>
#include <unordered_map>

#include "AKIpUptaneMes.h"
#include "asn1/asn1_message.h"

class MsgHandler {
 public:
  enum ReturnCode { kUnkownMsg = -1, kOk, kRebootRequired };

 public:
  MsgHandler() = default;
  virtual ~MsgHandler() = default;
  MsgHandler(const MsgHandler&) = delete;
  MsgHandler(const MsgHandler&&) = delete;
  MsgHandler& operator=(const MsgHandler&) = delete;
  MsgHandler& operator=(const MsgHandler&&) = delete;

 public:
  virtual ReturnCode handleMsg(const Asn1Message::Ptr& in_msg, Asn1Message::Ptr& out_msg) = 0;
};

class MsgDispatcher : public MsgHandler {
 public:
  using Handler = std::function<ReturnCode(Asn1Message&, Asn1Message&)>;

  void registerHandler(AKIpUptaneMes_PR msg_id, Handler handler);
  ReturnCode handleMsg(const Asn1Message::Ptr& in_msg, Asn1Message::Ptr& out_msg) override;

 private:
  std::unordered_map<unsigned int, Handler> handler_map_;
};

#endif  // MSG_HANDLER_H
