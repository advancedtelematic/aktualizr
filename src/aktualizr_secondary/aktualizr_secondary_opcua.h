#ifndef AKTUALIZR_SECONDARY_OPCUA_H_
#define AKTUALIZR_SECONDARY_OPCUA_H_

#include <memory>

#include "aktualizr_secondary_common.h"
#include "aktualizr_secondary_config.h"
#include "opcuaserver_secondary_delegate.h"

#include <opcuabridge/opcuabridgeserver.h>

#include <boost/shared_ptr.hpp>

class AktualizrSecondaryOpcua : private AktualizrSecondaryCommon {
 public:
  AktualizrSecondaryOpcua(const AktualizrSecondaryConfig&, boost::shared_ptr<INvStorage>&);
  void run();
  void stop();

 private:
  bool running_;
  OpcuaServerSecondaryDelegate delegate_;
  std::unique_ptr<opcuabridge::Server> server_;
};

#endif  // AKTUALIZR_SECONDARY_OPCUA_H_
