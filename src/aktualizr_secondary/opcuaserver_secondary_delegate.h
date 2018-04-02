#ifndef AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
#define AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_

#include <opcuabridge/opcuabridgeserver.h>
#include <uptane/tuf.h>
#include <utils.h>

class AktualizrSecondaryCommon;

class OpcuaServerSecondaryDelegate : public opcuabridge::ServerDelegate {
 public:
  OpcuaServerSecondaryDelegate(AktualizrSecondaryCommon*);

  void handleServerInitialized(opcuabridge::ServerModel*) override;
  void handleVersionReportRequested(opcuabridge::ServerModel*) override;
  void handleMetaDataFileReceived(opcuabridge::ServerModel*) override;
  void handleAllMetaDataFilesReceived(opcuabridge::ServerModel*) override;
  void handleDirectoryFilesSynchronized(opcuabridge::ServerModel*) override;

 private:
  AktualizrSecondaryCommon* secondary_;
  Uptane::MetaPack received_meta_pack_;
  TemporaryDirectory ostree_sync_working_repo_dir_;
};

#endif  // AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
