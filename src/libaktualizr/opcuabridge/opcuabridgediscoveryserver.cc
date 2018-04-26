#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>

#include "logging/logging.h"

#include "opcuabridgediscoveryserver.h"

namespace ba = boost::asio;

namespace opcuabridge {
namespace discovery {

Server::Server(EndPointServiceType type, uint16_t port) : socket_(io_service_) {
  ba::ip::udp::endpoint listen_endpoint(ba::ip::address_v6::any(), port);
  socket_.open(listen_endpoint.protocol());
  socket_.set_option(ba::ip::udp::socket::reuse_address(true));
  socket_.bind(listen_endpoint);
  init(type);
}

Server::Server(EndPointServiceType type, int socket_fd, uint16_t port) : socket_(io_service_) {
  socket_.assign(ba::ip::udp::v6(), socket_fd);
  init(type);
}

void Server::init(EndPointServiceType type) {
  socket_.set_option(
      ba::ip::multicast::join_group(ba::ip::address_v6::from_string(OPCUA_DISCOVERY_SERVICE_MULTICAST_ADDR)));

  response_ = (type == kNoLDS ? no_lds_response_ : type == kLDS ? lds_response_ : discovery_service_data_type());
}

bool Server::run(const volatile bool* running) {
  try {
    while (running != nullptr) {
      discovery_service_data_type data;
      ba::ip::udp::endpoint sender_endpoint;
      socket_.receive_from(ba::buffer(data), sender_endpoint);
      socket_.send_to(ba::buffer(response_), sender_endpoint);
    }
  } catch (std::exception& ex) {
    LOG_ERROR << "OPC-UA discovery server: " << ex.what();
    return false;
  }
  return true;
}

}  // namespace discovery
}  // namespace opcuabridge
