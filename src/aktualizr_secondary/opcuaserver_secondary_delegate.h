#ifndef AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
#define AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_

#include <opcuabridge/opcuabridgeserver.h>

class OpcuaServerSecondaryDelegate : public opcuabridge::ServerDelegate {
 public:
  void handleServerInitialized(opcuabridge::ServerModel*) override;
  void handleVersionReportRequested(opcuabridge::ServerModel*) override;
  void handleMetaDataFilesReceived(opcuabridge::ServerModel*) override;
  void handleDirectoryFilesSynchronized(opcuabridge::ServerModel*) override;
};

#endif  // AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
