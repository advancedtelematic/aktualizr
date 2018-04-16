#ifndef IP_UPTANE_CONNECTION_H_
#define IP_UPTANE_CONNECTION_H_

#include <unistd.h>
#include <memory>
#include <thread>

#include "secondary_ipc/aktualizr_secondary_ipc.h"
#include "utilities/utils.h"

class IpUptaneConnection {
 public:
  IpUptaneConnection(in_port_t in_port, struct in6_addr = IN6ADDR_ANY_INIT);
  ~IpUptaneConnection();
  IpUptaneConnection(const IpUptaneConnection&) = delete;
  IpUptaneConnection& operator=(const IpUptaneConnection&) = delete;

  SecondaryPacket::ChanType in_channel_;
  SecondaryPacket::ChanType out_channel_;
  in_port_t listening_port() const;
  sockaddr_storage listening_address() const;
  void stop();

 private:
  void handle_connection_msgs(SocketHandle con, std::unique_ptr<sockaddr_storage> addr);
  void open_socket();
  SocketHandle socket_hdl_;
  std::thread in_thread_;
  std::thread out_thread_;
  in_port_t in_port_;
  struct in6_addr in_addr_;
};

#endif  // IP_UPTANE_CONNECTION_H_
