#ifndef AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
#define AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_

#include <opcuabridge/opcuabridgeserver.h>

class AktualizrSecondaryCommon;

class OpcuaServerSecondaryDelegate : public opcuabridge::ServerDelegate {
 public:
  OpcuaServerSecondaryDelegate(AktualizrSecondaryCommon*);

  void handleServerInitialized(opcuabridge::ServerModel*) override;
  void handleVersionReportRequested(opcuabridge::ServerModel*) override;
  void handleMetaDataFilesReceived(opcuabridge::ServerModel*) override;
  void handleDirectoryFilesSynchronized(opcuabridge::ServerModel*) override;

 private:
  AktualizrSecondaryCommon* secondary_;
};

#endif  // AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
