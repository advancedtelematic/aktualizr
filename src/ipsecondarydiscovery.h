#ifndef IPSECONDARYDISCOVERY_H_
#define IPSECONDARYDISCOVERY_H_

#include <vector>

#include "config.h"
#include "uptane/secondaryconfig.h"

const int AKT_DISCOVERY_REQ = 0x0;
const int AKT_DISCOVERY_RESP = 0x80;

class IpSecondaryDiscovery {
 public:
  IpSecondaryDiscovery(const NetworkConfig &config) : config_(config){};
  std::vector<Uptane::SecondaryConfig> discover();
  std::vector<Uptane::SecondaryConfig> waitDevices();
  void sendRequest();

 private:
  NetworkConfig config_;
};

#endif  // IPSECONDARYDISCOVERY_H_
