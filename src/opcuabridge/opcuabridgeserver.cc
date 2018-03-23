#include "opcuabridgeserver.h"
#include "opcuabridgediscoveryserver.h"

#include <open62541.h>

#include <uptane/secondaryconfig.h>
#include <utils.h>

#include <boost/bind.hpp>
#include <boost/preprocessor/array/to_list.hpp>
#include <boost/preprocessor/list/for_each.hpp>

#include <boost/thread/scoped_thread.hpp>

namespace opcuabridge {

ServerModel::ServerModel(UA_Server* server) {
#define INIT_SERVER_NODESET(r, SERVER, e) e.InitServerNodeset(SERVER);

  BOOST_PP_LIST_FOR_EACH(INIT_SERVER_NODESET, server,
                         BOOST_PP_ARRAY_TO_LIST((6, (configuration_, version_report_, metadatafiles_, metadatafile_,
                                                     file_list_, file_data_))));
}

Server::Server(ServerDelegate* delegate, uint16_t port) : delegate_(delegate) {
  server_config_ = UA_ServerConfig_new_minimal(port, NULL);
  server_config_->logger = &opcuabridge::BoostLogOpcua;

  server_ = UA_Server_new(server_config_);
  model_ = new ServerModel(server_);

  model_->file_list_.setOnAfterWriteCallback(boost::bind(&Server::onFileListUpdated, this, _1));
  model_->metadatafile_.setOnAfterWriteCallback(boost::bind(&Server::countReceivedMetadataFile, this, _1));
  model_->version_report_.setOnBeforeReadCallback(boost::bind(&Server::onVersionReportRequested, this, _1));

  if (delegate_) delegate_->handleServerInitialized(model_);
}

Server::~Server() {
  delete model_;
  UA_Server_delete(server_);
  UA_ServerConfig_delete(server_config_);
}

bool Server::run(volatile bool* running) {
  boost::scoped_thread<boost::interrupt_and_join_if_joinable> discovery(
      [](volatile bool* server_running, ServerDelegate* delegate) {
        discovery::Server discovery_server(delegate->getServiceType(), delegate->getDiscoveryPort());
        discovery_server.run(server_running);
      },
      running, delegate_);
  return (UA_STATUSCODE_GOOD == UA_Server_run(server_, running));
}

void Server::onVersionReportRequested(VersionReport* version_report) {
  if (delegate_) delegate_->handleVersionReportRequested(model_);
}

void Server::onFileListUpdated(FileList* file_list) {
  if (!file_list->getBlock().empty() && file_list->getBlock()[0] == '\0' && delegate_)
    delegate_->handleDirectoryFilesSynchronized(model_);
}

void Server::countReceivedMetadataFile(MetadataFile* metadata_file) {
  if (model_->metadatafiles_.getGUID() == metadata_file->getGUID()) ++model_->received_metadata_files_;
  if (model_->metadatafiles_.getNumberOfMetadataFiles() == model_->received_metadata_files_ && delegate_) {
    model_->received_metadata_files_ = 0;
    delegate_->handleMetaDataFilesReceived(model_);
  }
}

}  // namespace opcuabridge
