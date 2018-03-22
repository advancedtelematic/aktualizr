#include "opcuaserver_secondary_delegate.h"

void OpcuaServerSecondaryDelegate::handleServerInitialized(opcuabridge::ServerModel* model) {
  model->configuration_.setSerial("");  // sconfig.ecu_serial
  model->configuration_.setHwId("");    // sconfig.ecu_hardware_id
  model->configuration_.setPublicKeyType(kRSA2048);
  model->configuration_.setPublicKey(
      "");  // Utils::readFile((sconfig.full_client_dir / sconfig.ecu_public_key).string()));
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
