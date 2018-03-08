#ifndef AKTUALIZR_SECONDARY_DISCOVERY_H
#define AKTUALIZR_SECONDARY_DISCOVERY_H

#include "aktualizr_secondary_config.h"

#include "utils.h"

class AktualizrSecondaryDiscovery {
 public:
  AktualizrSecondaryDiscovery(const AktualizrSecondaryNetConfig &config);

  int listening_port() const;
  void run();
  void stop();

 private:
  void open_socket();

  const AktualizrSecondaryNetConfig &config_;
  SocketHandle socket_hdl_;
};

#endif  // AKTUALIZR_SECONDARY_DISCOVERY_H
