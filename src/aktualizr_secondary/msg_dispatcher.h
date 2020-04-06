#ifndef MSGDISPATCHER_H
#define MSGDISPATCHER_H

#include <memory>
#include <unordered_map>

#include "AKIpUptaneMes.h"
#include "asn1/asn1_message.h"

#include "aktualizr_secondary_interface.h"

class MsgDispatcher {
 public:
  enum HandleStatusCode { kUnkownMsg = -1, kOk, kRebootRequired };

  using Handler = std::function<HandleStatusCode(Asn1Message&, Asn1Message&)>;

 public:
  MsgDispatcher() = default;
  virtual ~MsgDispatcher() = default;

  MsgDispatcher(const MsgDispatcher&) = delete;
  MsgDispatcher(const MsgDispatcher&&) = delete;
  MsgDispatcher& operator=(const MsgDispatcher&) = delete;
  MsgDispatcher& operator=(const MsgDispatcher&&) = delete;

 public:
  virtual void registerHandler(AKIpUptaneMes_PR msg_id, Handler handler);
  virtual HandleStatusCode handleMsg(const Asn1Message::Ptr& in_msg, Asn1Message::Ptr& out_msg);

 private:
  std::unordered_map<unsigned int, MsgDispatcher::Handler> handler_map_;
};

class AktualizrSecondaryMsgDispatcher : public MsgDispatcher {
 public:
  AktualizrSecondaryMsgDispatcher(IAktualizrSecondary& secondary);

 protected:
  HandleStatusCode getInfoHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  HandleStatusCode getManifestHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  HandleStatusCode putMetaHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  HandleStatusCode sendFirmwareHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  HandleStatusCode sendFirmwareDataHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  HandleStatusCode installHdlr(Asn1Message& in_msg, Asn1Message& out_msg);

 private:
  IAktualizrSecondary& secondary_;
};

#endif  // MSGDISPATCHER_H
