#ifndef AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
#define AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_

#include <opcuabridge/opcuabridgeserver.h>

class OpcuaServerSecondaryDelegate : public opcuabridge::ServerDelegate {
 public:
  virtual void handleServerInitialized(opcuabridge::ServerModel*) override;
  virtual void handleVersionReportRequested(opcuabridge::ServerModel*) override;
  virtual void handleMetaDataFilesReceived(opcuabridge::ServerModel*) override;
  virtual void handleDirectoryFilesSynchronized(opcuabridge::ServerModel*) override;
};

#endif  // AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
