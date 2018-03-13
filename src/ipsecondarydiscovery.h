#ifndef IPSECONDARYDISCOVERY_H_
#define IPSECONDARYDISCOVERY_H_

#include "config.h"

class IpSecondaryDiscovery {
    public:
        IpSecondaryDiscovery(const NetworkConfig &config): config_(config) {};
        int discover();
    private:
        NetworkConfig config_;

};

#endif //IPSECONDARYDISCOVERY_H_
