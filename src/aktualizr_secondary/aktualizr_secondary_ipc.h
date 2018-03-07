#ifndef AKTUALIZR_SECONDARY_IPC_H
#define AKTUALIZR_SECONDARY_IPC_H

#include <memory>
#include <string>

#include "channel.h"

struct SecondaryMessage {
  virtual std::string str() const = 0;
};

struct GetVersionMessage : public SecondaryMessage {
  std::string str() const override { return "GetVersion"; }
};

struct SecondaryPacket {
  SecondaryPacket(sockaddr_storage peer, SecondaryMessage *message)
      : peer_addr(peer), msg(std::unique_ptr<SecondaryMessage>(message)) {}

  std::string str() const { return "Packet: " + msg->str(); }

  using ChanType = Channel<std::shared_ptr<SecondaryPacket>>;

  sockaddr_storage peer_addr;
  std::unique_ptr<SecondaryMessage> msg;
};

#endif  // AKTUALIZR_SECONDARY_IPC_H
