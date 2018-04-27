#ifndef OPCUABRIDGE_SERVER_H_
#define OPCUABRIDGE_SERVER_H_

#include "opcuabridge.h"
#include "opcuabridgeconfig.h"
#include "opcuabridgediscoverytypes.h"

namespace opcuabridge {

class Server;

struct ServerModel {
  explicit ServerModel(UA_Server* /*server*/);

  Configuration configuration_;

  VersionReport version_report_;

  MetadataFiles metadatafiles_;
  MetadataFile metadatafile_;

  FileList file_list_;
  FileData file_data_;

  OriginalManifest original_manifest_;

  friend class Server;

 private:
  std::size_t received_metadata_files_ = 0;
};

class ServerDelegate {
 public:
  void setServiceType(discovery::EndPointServiceType service_type) { service_type_ = service_type; }
  const discovery::EndPointServiceType& getServiceType() const { return service_type_; }

  void setDiscoveryPort(uint16_t discovery_port) { discovery_port_ = discovery_port; }
  const uint16_t& getDiscoveryPort() const { return discovery_port_; }

  virtual ~ServerDelegate() = default;
  virtual void handleServerInitialized(ServerModel*) = 0;
  virtual void handleVersionReportRequested(ServerModel*) = 0;      // on version report is requested
  virtual void handleOriginalManifestRequested(ServerModel*) = 0;   // on original manifest is requested
  virtual void handleMetaDataFileReceived(ServerModel*) = 0;        // on after each metadata file recv.
  virtual void handleAllMetaDataFilesReceived(ServerModel*) = 0;    // after all metadata files recv.
  virtual void handleDirectoryFilesSynchronized(ServerModel*) = 0;  // when dir. sync. finished
  virtual void handleDirectoryFileListRequested(ServerModel*) = 0;  // on dir. file list is requested

 private:
  discovery::EndPointServiceType service_type_ = discovery::kNoLDS;
  uint16_t discovery_port_ = OPCUA_DISCOVERY_SERVICE_PORT;
};

class Server {
 public:
  explicit Server(ServerDelegate* /*delegate*/, uint16_t port = OPCUABRIDGE_PORT);
  Server(ServerDelegate* delegate, int socket_fd, int discovery_socket_fd, uint16_t port = OPCUABRIDGE_PORT);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  bool run(volatile bool* /*running*/);

 private:
  void initializeModel();
  void onFileListUpdated(FileList* /*file_list*/);
  void onFileListRequested(FileList* /*file_list*/);
  void countReceivedMetadataFile(MetadataFile* /*metadata_file*/);
  void onVersionReportRequested(VersionReport* /*version_report*/);
  void onOriginalManifestRequested(OriginalManifest* /*version_report*/);

  ServerModel* model_{};

  ServerDelegate* delegate_;

  UA_Server* server_;
  UA_ServerConfig* server_config_;

  int discovery_socket_fd_{};
  bool use_socket_activation_ = false;
};

}  // namespace opcuabridge

#endif  // OPCUABRIDGE_SERVER_H_
