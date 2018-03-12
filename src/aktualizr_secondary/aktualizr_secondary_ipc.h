#ifndef AKTUALIZR_SECONDARY_IPC_H
#define AKTUALIZR_SECONDARY_IPC_H

#include <memory>
#include <string>

#include <sys/socket.h>

#include "channel.h"

struct SecondaryMessage {
  std::string variant;
};

struct GetVersionMessage : public SecondaryMessage {
  GetVersionMessage() { variant = "GetVersion"; }
};

struct StopMessage : public SecondaryMessage {
  StopMessage() { variant = "Stop"; }
};

struct SecondaryPacket {
  SecondaryPacket(SecondaryMessage *message) : msg(std::unique_ptr<SecondaryMessage>(message)) {
    memset(&peer_addr, 0, sizeof(peer_addr));
  }
  SecondaryPacket(sockaddr_storage peer, SecondaryMessage *message)
      : peer_addr(peer), msg(std::unique_ptr<SecondaryMessage>(message)) {}

  std::string str() const { return "Packet: " + msg->variant; }

  using ChanType = Channel<std::shared_ptr<SecondaryPacket>>;

  sockaddr_storage peer_addr;
  std::unique_ptr<SecondaryMessage> msg;
};

#endif  // AKTUALIZR_SECONDARY_IPC_H
