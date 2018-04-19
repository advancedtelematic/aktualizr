#ifndef IPSECONDARYDISCOVERY_H_
#define IPSECONDARYDISCOVERY_H_

#include <vector>

#include "config.h"
#include "uptane/secondaryconfig.h"
#include "utilities/utils.h"

const int AKT_DISCOVERY_REQ = 0x00;
const int AKT_DISCOVERY_RESP = 0x01;

class IpSecondaryDiscovery {
 public:
  IpSecondaryDiscovery(const NetworkConfig &config) : config_(config){};
  std::vector<Uptane::SecondaryConfig> discover();
  std::vector<Uptane::SecondaryConfig> waitDevices();
  bool sendRequest();

 private:
  NetworkConfig config_;
  SocketHandle socket_hdl;
};

#endif  // IPSECONDARYDISCOVERY_H_
