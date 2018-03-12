#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_ipc.h"
#include "channel.h"

struct SocketCloser {
  void operator()(int *ptr) const { close(*ptr); }
};

using SocketHandle = std::unique_ptr<int, SocketCloser>;

class AktualizrSecondary {
 public:
  AktualizrSecondary(const AktualizrSecondaryConfig &config);
  void run();

 private:
  void handle_connection_msgs(int con_fd, std::unique_ptr<sockaddr_storage> addr);
  void open_socket();
  AktualizrSecondaryConfig config_;
  SocketHandle socket_hdl_;
  SecondaryPacket::ChanType channel_;
};

#endif  // AKTUALIZR_SECONDARY_H
