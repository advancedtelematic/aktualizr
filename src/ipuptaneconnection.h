#ifndef IP_UPTANE_CONNECTION_H_
#define IP_UPTANE_CONNECTION_H_

#include <unistd.h>
#include <memory>
#include <thread>

#include "aktualizr_secondary_ipc.h"
#include "utils.h"

using SocketHandle = std::unique_ptr<int, SocketCloser>;

class IpUptaneConnection {
 public:
  IpUptaneConnection(int in_port);
  SecondaryPacket::ChanType in_channel_;
  SecondaryPacket::ChanType out_channel_;
  int listening_port() const;
  void stop();

 private:
  void handle_connection_msgs(SocketHandle con, std::unique_ptr<sockaddr_storage> addr);
  void open_socket();
  SocketHandle socket_hdl_;
  std::thread in_thread_;
  int in_port_;
};

#endif  // IP_UPTANE_CONNECTION_H_
