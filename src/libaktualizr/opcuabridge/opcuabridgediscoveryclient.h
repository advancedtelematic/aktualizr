#ifndef OPCUABRIDGE_DISCOVERYCLIENT_H_
#define OPCUABRIDGE_DISCOVERYCLIENT_H_

#include <list>
#include <string>
#include <unordered_map>

#include "opcuabridgeconfig.h"
#include "opcuabridgediscoverytypes.h"

namespace opcuabridge {
namespace discovery {

class Client {
 public:
  explicit Client(uint16_t port) : port_(port) {}

  struct discovered_endpoint {
    std::string address;
    EndPointServiceType type;
  };
  typedef std::list<discovered_endpoint> endpoints_list_type;

  endpoints_list_type getDiscoveredEndPoints();

 private:
  void collectDiscoveredEndPointsOnIface(unsigned int /*iface*/);

  std::unordered_map<std::string, EndPointServiceType> discovered_endpoints_;

  const discovery_service_data_type lds_response_ = OPCUA_DISCOVERY_SERVICE_LDS_RESPONSE,
                                    no_lds_response_ = OPCUA_DISCOVERY_SERVICE_RESPONSE;

  uint16_t port_;
};

}  // namespace discovery
}  // namespace opcuabridge

#endif  // OPCUABRIDGE_DISCOVERYCLIENT_H_
