#include "opcuabridgeserver.h"
#include "opcuabridgediscoveryserver.h"

#include <open62541.h>

#include <uptane/secondaryconfig.h>
#include "utilities/utils.h"

#include <boost/preprocessor/array/to_list.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <functional>

#include <boost/smart_ptr/make_unique.hpp>
#include <boost/thread/scoped_thread.hpp>

namespace opcuabridge {

ServerModel::ServerModel(UA_Server* server) {
#define INIT_SERVER_NODESET(r, SERVER, e) e.InitServerNodeset(SERVER);

  BOOST_PP_LIST_FOR_EACH(INIT_SERVER_NODESET, server,
                         BOOST_PP_ARRAY_TO_LIST((6, (configuration_, version_report_, metadatafiles_, metadatafile_,
                                                     file_list_, file_data_, original_manifest_))));
}

Server::Server(ServerDelegate* delegate, uint16_t port) : delegate_(delegate) {
  server_config_ = UA_ServerConfig_new_minimal(port, nullptr);
  server_config_->logger = &opcuabridge::BoostLogOpcua;

  server_ = UA_Server_new(server_config_);

  initializeModel();

  if (delegate_ != nullptr) {
    delegate_->handleServerInitialized(model_);
  }
}

Server::Server(ServerDelegate* delegate, int socket_fd, int discovery_socket_fd, uint16_t port) : delegate_(delegate) {
  server_config_ = UA_ServerConfig_new_minimal(port, nullptr);
  server_config_->logger = &opcuabridge::BoostLogOpcua;

  server_config_->networkLayers[0].deleteMembers(&server_config_->networkLayers[0]);
  server_config_->networkLayers[0] = UA_ServerNetworkLayerTCPSocketActivation(UA_ConnectionConfig_default, socket_fd);
  server_config_->networkLayersSize = 1;

  discovery_socket_fd_ = discovery_socket_fd;
  use_socket_activation_ = true;

  server_ = UA_Server_new(server_config_);

  initializeModel();

  if (delegate_ != nullptr) {
    delegate_->handleServerInitialized(model_);
  }
}

void Server::initializeModel() {
  using std::placeholders::_1;

  model_ = new ServerModel(server_);

  model_->file_list_.setOnBeforeReadCallback(std::bind(&Server::onFileListRequested, this, _1));
  model_->file_list_.setOnAfterWriteCallback(std::bind(&Server::onFileListUpdated, this, _1));
  model_->metadatafile_.setOnAfterWriteCallback(std::bind(&Server::countReceivedMetadataFile, this, _1));
  model_->version_report_.setOnBeforeReadCallback(std::bind(&Server::onVersionReportRequested, this, _1));
  model_->original_manifest_.setOnBeforeReadCallback(std::bind(&Server::onOriginalManifestRequested, this, _1));
}

Server::~Server() {
  delete model_;
  UA_Server_delete(server_);
  UA_ServerConfig_delete(server_config_);
}

bool Server::run(volatile bool* running) {
  boost::scoped_thread<boost::interrupt_and_join_if_joinable> discovery(
      [](bool use_socket_activation, int socket_fd, volatile bool* server_running, ServerDelegate* delegate) {
        std::unique_ptr<discovery::Server> discovery_server;
        if (use_socket_activation) {
          discovery_server = boost::make_unique<discovery::Server>(delegate->getServiceType(), socket_fd,
                                                                   delegate->getDiscoveryPort());
        } else {
          discovery_server =
              boost::make_unique<discovery::Server>(delegate->getServiceType(), delegate->getDiscoveryPort());
        }
        discovery_server->run(server_running);
      },
      use_socket_activation_, discovery_socket_fd_, running, delegate_);
  return (UA_STATUSCODE_GOOD == UA_Server_run(server_, running));
}

void Server::onVersionReportRequested(VersionReport* version_report) {
  if (delegate_ != nullptr) {
    delegate_->handleVersionReportRequested(model_);
  }
}

void Server::onOriginalManifestRequested(OriginalManifest* version_report) {
  if (delegate_ != nullptr) {
    delegate_->handleOriginalManifestRequested(model_);
  }
}

void Server::onFileListRequested(FileList* file_list) {
  if (delegate_ != nullptr) {
    delegate_->handleDirectoryFileListRequested(model_);
  }
}

void Server::onFileListUpdated(FileList* file_list) {
  if (!file_list->getBlock().empty() && file_list->getBlock()[0] == '\0' && (delegate_ != nullptr)) {
    delegate_->handleDirectoryFilesSynchronized(model_);
  }
}

void Server::countReceivedMetadataFile(MetadataFile* metadata_file) {
  if (model_->metadatafiles_.getGUID() == metadata_file->getGUID()) {
    ++model_->received_metadata_files_;
    if (delegate_ != nullptr) {
      delegate_->handleMetaDataFileReceived(model_);
    }
  }
  if (model_->metadatafiles_.getNumberOfMetadataFiles() == model_->received_metadata_files_ && (delegate_ != nullptr)) {
    model_->received_metadata_files_ = 0;
    delegate_->handleAllMetaDataFilesReceived(model_);
  }
}

}  // namespace opcuabridge
