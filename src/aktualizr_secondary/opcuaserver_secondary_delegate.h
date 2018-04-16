#ifndef AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
#define AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_

#include <opcuabridge/opcuabridgeserver.h>
#include <uptane/tuf.h>
#include "utilities/utils.h"

#include <boost/filesystem/path.hpp>

class AktualizrSecondaryCommon;

class OpcuaServerSecondaryDelegate : public opcuabridge::ServerDelegate {
 public:
  explicit OpcuaServerSecondaryDelegate(AktualizrSecondaryCommon* /*secondary*/);

  void handleServerInitialized(opcuabridge::ServerModel* model) override;
  void handleVersionReportRequested(opcuabridge::ServerModel* model) override;
  void handleMetaDataFileReceived(opcuabridge::ServerModel* model) override;
  void handleAllMetaDataFilesReceived(opcuabridge::ServerModel* model) override;
  void handleDirectoryFilesSynchronized(opcuabridge::ServerModel* model) override;
  void handleOriginalManifestRequested(opcuabridge::ServerModel* model) override;
  void handleDirectoryFileListRequested(opcuabridge::ServerModel* model) override;

 private:
  AktualizrSecondaryCommon* secondary_;
  Uptane::MetaPack received_meta_pack_;
  TemporaryDirectory ostree_sync_working_repo_dir_;
};

#endif  // AKTUALIZR_SECONDARY_OPCUASERVER_SECONDARY_DELEGATE_H_
