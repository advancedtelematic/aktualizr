#ifndef IPSECONDARYDISCOVERY_H_
#define IPSECONDARYDISCOVERY_H_

#include <vector>

#include "config.h"
#include "uptane/secondaryconfig.h"

const int AKT_DISCOVERY_REQ = 0x00;
const int AKT_DISCOVERY_RESP = 0x01;

class IpSecondaryDiscovery {
 public:
  IpSecondaryDiscovery(const NetworkConfig &config) : config_(config){};
  ~IpSecondaryDiscovery() { close(socket_fd); }
  std::vector<Uptane::SecondaryConfig> discover();
  std::vector<Uptane::SecondaryConfig> waitDevices();
  bool sendRequest();

 private:
  NetworkConfig config_;
  int socket_fd;
};

#endif  // IPSECONDARYDISCOVERY_H_
