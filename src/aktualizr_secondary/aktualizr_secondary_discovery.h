#ifndef AKTUALIZR_SECONDARY_DISCOVERY_H
#define AKTUALIZR_SECONDARY_DISCOVERY_H

#include "aktualizr_secondary.h"
#include "aktualizr_secondary_config.h"

#include "utils.h"

class AktualizrSecondaryDiscovery {
 public:
  AktualizrSecondaryDiscovery(const AktualizrSecondaryNetConfig &config, const AktualizrSecondary &akt_secondary);

  int listening_port() const;
  void run();
  void stop();

 private:
  void open_socket();

  const AktualizrSecondaryNetConfig &config_;
  const AktualizrSecondary &akt_secondary_;
  SocketHandle socket_hdl_;
};

#endif  // AKTUALIZR_SECONDARY_DISCOVERY_H
