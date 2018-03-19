#ifndef AKTUALIZR_SECONDARY_OPCUA_H_
#define AKTUALIZR_SECONDARY_OPCUA_H_

#include <memory>

#include "aktualizr_secondary_config.h"

#include "opcuaserver_secondary_delegate.h"

#include <opcuabridge/opcuabridgeserver.h>

class AktualizrSecondaryOpcua {
 public:
  AktualizrSecondaryOpcua(const AktualizrSecondaryConfig &config);
  void run();
  void stop();

 private:
  bool running_;
  AktualizrSecondaryConfig config_;
  OpcuaServerSecondaryDelegate delegate_;
  std::unique_ptr<opcuabridge::Server> server_;
};

#endif  // AKTUALIZR_SECONDARY_OPCUA_H_
