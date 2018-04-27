#ifndef OPCUABRIDGE_DISCOVERYSERVER_H_
#define OPCUABRIDGE_DISCOVERYSERVER_H_

#include <boost/asio.hpp>

#include "opcuabridgeconfig.h"
#include "opcuabridgediscoverytypes.h"

namespace opcuabridge {
namespace discovery {

class Server {
 public:
  Server(EndPointServiceType type, uint16_t port);
  Server(EndPointServiceType type, int socket_fd, uint16_t port);

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  bool run(const volatile bool* /*running*/);

 private:
  void init(EndPointServiceType /*type*/);

  boost::asio::io_service io_service_;
  boost::asio::ip::udp::socket socket_;
  discovery_service_data_type response_{};

  const discovery_service_data_type lds_response_ = OPCUA_DISCOVERY_SERVICE_LDS_RESPONSE,
                                    no_lds_response_ = OPCUA_DISCOVERY_SERVICE_RESPONSE;
};

}  // namespace discovery
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_DISCOVERYSERVER_H_
