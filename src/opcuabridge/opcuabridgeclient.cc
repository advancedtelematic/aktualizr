#include "opcuabridgeclient.h"

#include <uptane/secondaryconfig.h>

#include <open62541.h>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace opcuabridge {

SelectEndPoint::SelectEndPoint(const Uptane::SecondaryConfig& sconfig) {
  uri = sconfig.opcua_lds_uri;
}

Client::Client(const SelectEndPoint& selector) noexcept {
  UA_ClientConfig config = UA_ClientConfig_default;
  config.timeout = OPCUABRIDGE_CLIENT_SYNC_RESPONSE_TIMEOUT;
  config.logger = &opcuabridge::BoostLogOpcua;
  client_ = UA_Client_new(config);
  UA_Client_connect(client_, selector.uri.c_str());
}

Client::~Client() {
  UA_Client_disconnect(client_);
  UA_Client_delete(client_);
}

Client::operator bool() const { return (UA_Client_getState(client_) != UA_CLIENTSTATE_DISCONNECTED); }

Configuration Client::recvConfiguration() const {
  Configuration configuration;
  if (UA_Client_getState(client_) != UA_CLIENTSTATE_DISCONNECTED) {
    configuration.ClientRead(client_);
  }
  return configuration;
}

VersionReport Client::recvVersionReport() const {
  VersionReport version_report;
  if (UA_Client_getState(client_) != UA_CLIENTSTATE_DISCONNECTED) {
    version_report.ClientRead(client_);
  }
  return version_report;
}

bool Client::sendMetadataFiles(std::vector<MetadataFile>& files) const {
  MetadataFiles metadatafiles;
  bool retval = true;
  if (UA_Client_getState(client_) != UA_CLIENTSTATE_DISCONNECTED) {
    metadatafiles.setNumberOfMetadataFiles(files.size());
    metadatafiles.ClientWrite(client_);

    for (auto f_it = files.begin(); f_it != files.end(); ++f_it) {
      if (UA_STATUSCODE_GOOD != f_it->ClientWrite(client_)) {
        retval = false;
        break;
      }
    }
  }
  if (!retval && UA_Client_getState(client_) != UA_CLIENTSTATE_DISCONNECTED) {
    metadatafiles.setNumberOfMetadataFiles(0);  // signal cancel to secondary
    metadatafiles.ClientWrite(client_);
  }
  return retval;
}

bool Client::syncDirectoryFiles(const fs::path& repo_dir) const {
  if (UA_Client_getState(client_) == UA_CLIENTSTATE_DISCONNECTED) return false;

  opcuabridge::FileList file_list;
  opcuabridge::FileData file_data(repo_dir);

  if (UA_STATUSCODE_GOOD != file_list.ClientRead(client_)) return false;

  opcuabridge::FileUnorderedSet file_unordered_set;
  opcuabridge::UpdateFileUnorderedSet(&file_unordered_set, file_list);

  bool retval = true;
  fs::recursive_directory_iterator repo_dir_it_end, repo_dir_it(repo_dir);
  for (; repo_dir_it != repo_dir_it_end; ++repo_dir_it) {
    const fs::path& ent_path = repo_dir_it->path();
    if (fs::is_regular_file(ent_path)) {
      fs::path rel_path = fs::relative(ent_path, repo_dir);
      if (file_unordered_set.count((opcuabridge::FileSetEntry)rel_path.c_str()) == 0) {
        file_data.setFilePath(rel_path);
        if (UA_STATUSCODE_GOOD != file_data.ClientWriteFile(client_, ent_path)) {
          retval = false;
          break;
        }
      }
    }
  }
  file_list.getBlock().resize(1);
  file_list.getBlock()[0] = '\0';
  return ((UA_STATUSCODE_GOOD == file_list.ClientWrite(client_)) && retval);
}

}  // namespace opcuabridge
