#include "opcuaserver_secondary_delegate.h"
#include "aktualizr_secondary_common.h"

#include <logging.h>

OpcuaServerSecondaryDelegate::OpcuaServerSecondaryDelegate(AktualizrSecondaryCommon* secondary)
    : secondary_(secondary) {}

void OpcuaServerSecondaryDelegate::handleServerInitialized(opcuabridge::ServerModel* model) {
  if (!secondary_->uptaneInitialize()) {
    LOG_ERROR << "Secondary: failed to initialize";
    return;
  }
  model->configuration_.setSerial(secondary_->ecu_serial_);
  model->configuration_.setHwId(secondary_->hardware_id_);
  model->configuration_.setPublicKeyType(secondary_->config_.uptane.key_type);
  model->configuration_.setPublicKey(secondary_->keys_.getUptanePublicKey());
}

void OpcuaServerSecondaryDelegate::handleVersionReportRequested(opcuabridge::ServerModel* model) {
  // get the manifest of the installed image and fill the report
}

void OpcuaServerSecondaryDelegate::handleMetaDataFilesReceived(opcuabridge::ServerModel* model) {
  // extract the reference to update the secondaries repo when synchronization is finished
}

void OpcuaServerSecondaryDelegate::handleDirectoryFilesSynchronized(opcuabridge::ServerModel* model) {
  // checkout the appropriate revision (extracted from metadata file) and deploy
}
