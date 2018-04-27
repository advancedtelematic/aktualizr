#ifndef AKTUALIZR_SECONDARY_OPCUA_H_
#define AKTUALIZR_SECONDARY_OPCUA_H_

#include <memory>

#include "aktualizr_secondary_common.h"
#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_interface.h"
#include "opcuaserver_secondary_delegate.h"

#include <opcuabridgeserver.h>

#include <boost/shared_ptr.hpp>

class AktualizrSecondaryOpcua : public AktualizrSecondaryInterface, private AktualizrSecondaryCommon {
 public:
  AktualizrSecondaryOpcua(const AktualizrSecondaryConfig& /*config*/, std::shared_ptr<INvStorage>& /*storage*/);
  void run() override;
  void stop() override;

 private:
  bool running_;
  OpcuaServerSecondaryDelegate delegate_;
  std::unique_ptr<opcuabridge::Server> server_;
};

#endif  // AKTUALIZR_SECONDARY_OPCUA_H_
