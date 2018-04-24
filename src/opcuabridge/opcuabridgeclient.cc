#include "opcuabridgeclient.h"
#include "opcuabridgediscoveryclient.h"

#include "logging/logging.h"

#include "uptane/secondaryconfig.h"

#include <open62541.h>

#include <cstdlib>
#include <ctime>
#include <random>
#include <unordered_set>

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>

namespace fs = boost::filesystem;

namespace opcuabridge {

SelectEndPoint::DiscoveredEndPointCacheEntryHash::result_type SelectEndPoint::DiscoveredEndPointCacheEntryHash::
operator()(SelectEndPoint::DiscoveredEndPointCacheEntryHash::argument_type const& e) const {
  result_type seed = 0;
  boost::hash_combine(seed, e.serial);
  boost::hash_combine(seed, e.hwid);
  return seed;
}

bool SelectEndPoint::DiscoveredEndPointCacheEntryEqual::operator()(
    const SelectEndPoint::DiscoveredEndPointCacheEntryEqual::argument_type& lhs,
    const SelectEndPoint::DiscoveredEndPointCacheEntryEqual::argument_type& rhs) const {
  return (lhs.serial == rhs.serial && lhs.hwid == rhs.hwid);
}

thread_local SelectEndPoint::DiscoveredEndPointCache SelectEndPoint::discovered_end_points_cache_;

SelectEndPoint::SelectEndPoint(const Uptane::SecondaryConfig& sconfig) {
  if (discovered_end_points_cache_.empty()) {
    if (!sconfig.opcua_lds_url.empty()) {
      considerLdsRegisteredEndPoints(sconfig.opcua_lds_url);
    } else {
      discovery::Client discovery_client(OPCUA_DISCOVERY_SERVICE_PORT);
      for (const discovery::Client::discovered_endpoint& ep : discovery_client.getDiscoveredEndPoints()) {
        if (ep.type == discovery::kLDS) {
          considerLdsRegisteredEndPoints(makeOpcuaServerUri(ep.address));
        } else if (ep.type == discovery::kNoLDS) {
          std::string opcua_server_url = makeOpcuaServerUri(ep.address);
          if (endPointConfirmed(opcua_server_url, sconfig)) {
            discovered_end_points_cache_.insert(
                DiscoveredEndPointCacheEntry{sconfig.ecu_serial, sconfig.ecu_hardware_id, opcua_server_url});
          }
        } else {
          LOG_INFO << "OPC-UA discover secondary: unsupported response from " << ep.address;
        }
      }
    }
  }
  auto ep_it = discovered_end_points_cache_.find(
      DiscoveredEndPointCacheEntry{sconfig.ecu_serial, sconfig.ecu_hardware_id, std::string()});
  if (ep_it != discovered_end_points_cache_.end()) {
    url_ = ep_it->opcua_server_url;
  }
}

bool SelectEndPoint::endPointConfirmed(const std::string& opcua_server_url,
                                       const Uptane::SecondaryConfig& sconfig) const {
  auto probe_opcua_server_config = Client(opcua_server_url).recvConfiguration();
  return (probe_opcua_server_config.getSerial() == sconfig.ecu_serial &&
          probe_opcua_server_config.getHwId() == sconfig.ecu_hardware_id);
}

std::string SelectEndPoint::makeOpcuaServerUri(const std::string& address) const {
  return "opc.tcp://[" + address + "]:" + std::to_string(OPCUABRIDGE_PORT);
}

void SelectEndPoint::considerLdsRegisteredEndPoints(const std::string& opcua_lds_url) {
  UA_ApplicationDescription* applicationDescriptionArray = nullptr;
  size_t applicationDescriptionArraySize = 0;

  UA_StatusCode retval;
  {
    UA_ClientConfig config = UA_ClientConfig_default;
    config.timeout = OPCUABRIDGE_CLIENT_SYNC_RESPONSE_TIMEOUT_MS;
    config.logger = &opcuabridge::BoostLogOpcua;

    UA_Client* client = UA_Client_new(config);
    retval = UA_Client_findServers(client, opcua_lds_url.c_str(), 0, nullptr, 0, nullptr,
                                   &applicationDescriptionArraySize, &applicationDescriptionArray);
    UA_Client_delete(client);
  }

  if (retval != UA_STATUSCODE_GOOD) {
    LOG_ERROR << "OPC-UA LDS dicovery request: unable to get registered servers on " << opcua_lds_url;
    return;
  }

  for (size_t i = 0; i < applicationDescriptionArraySize; i++) {
    UA_ApplicationDescription* description = &applicationDescriptionArray[i];
    if (description->applicationType != UA_APPLICATIONTYPE_SERVER) {
      continue;
    }
    if (description->discoveryUrlsSize == 0) {
      LOG_INFO << "OPC-UA server " << std::string(reinterpret_cast<char*>(description->applicationUri.data),
                                                  description->applicationUri.length)
               << " does not provide any discovery urls";
      continue;
    }

    UA_ClientConfig config = UA_ClientConfig_default;
    config.timeout = OPCUABRIDGE_CLIENT_SYNC_RESPONSE_TIMEOUT_MS;
    config.logger = &opcuabridge::BoostLogOpcua;
    UA_Client* client = UA_Client_new(config);

    std::string discovery_url(reinterpret_cast<char*>(description->discoveryUrls[0].data),
                              description->discoveryUrls[0].length);

    UA_EndpointDescription* endpointArray = nullptr;
    size_t endpointArraySize = 0;
    retval = UA_Client_getEndpoints(client, discovery_url.c_str(), &endpointArraySize, &endpointArray);

    if (retval != UA_STATUSCODE_GOOD) {
      UA_Client_disconnect(client);
      UA_Client_delete(client);
      break;
    }

    for (size_t j = 0; j < endpointArraySize; j++) {
      UA_EndpointDescription* endpoint = &endpointArray[j];
      std::string opcua_server_url =
          std::string(reinterpret_cast<char*>(endpoint->endpointUrl.data), endpoint->endpointUrl.length);
      auto opcua_server_config = Client(opcua_server_url).recvConfiguration();
      discovered_end_points_cache_.insert(DiscoveredEndPointCacheEntry{
          opcua_server_config.getSerial(), opcua_server_config.getHwId(), opcua_server_url});
    }
    UA_Array_delete(endpointArray, endpointArraySize, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    UA_Client_delete(client);
  }
  UA_Array_delete(applicationDescriptionArray, applicationDescriptionArraySize,
                  &UA_TYPES[UA_TYPES_APPLICATIONDESCRIPTION]);
}

Client::Client(const SelectEndPoint& selector) noexcept : Client(selector.getUrl()) {}

Client::Client(const std::string& url) noexcept {
  UA_ClientConfig config = UA_ClientConfig_default;
  config.timeout = OPCUABRIDGE_CLIENT_SYNC_RESPONSE_TIMEOUT_MS;
  config.logger = &opcuabridge::BoostLogOpcua;
  client_ = UA_Client_new(config);
  UA_Client_connect(client_, url.c_str());
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

OriginalManifest Client::recvOriginalManifest() const {
  OriginalManifest original_manifest;
  if (UA_Client_getState(client_) != UA_CLIENTSTATE_DISCONNECTED) {
    original_manifest.ClientRead(client_);
  }
  return original_manifest;
}

bool Client::sendMetadataFiles(std::vector<MetadataFile>& files) const {
  MetadataFiles metadatafiles;
  bool retval = true;
  if (UA_Client_getState(client_) != UA_CLIENTSTATE_DISCONNECTED) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, INT_MAX);
    metadatafiles.setGUID(dis(gen));
    metadatafiles.setNumberOfMetadataFiles(files.size());
    metadatafiles.ClientWrite(client_);

    for (auto f_it = files.begin(); f_it != files.end(); ++f_it) {
      f_it->setGUID(metadatafiles.getGUID());
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
  if (UA_Client_getState(client_) == UA_CLIENTSTATE_DISCONNECTED) {
    return false;
  }

  opcuabridge::FileList file_list;
  opcuabridge::FileData file_data(repo_dir);

  if (UA_STATUSCODE_GOOD != file_list.ClientRead(client_)) {
    return false;
  }

  opcuabridge::FileUnorderedSet file_unordered_set;
  opcuabridge::UpdateFileUnorderedSet(&file_unordered_set, file_list);

  bool retval = true;
  fs::recursive_directory_iterator repo_dir_it_end, repo_dir_it(repo_dir);
  for (; repo_dir_it != repo_dir_it_end; ++repo_dir_it) {
    const fs::path& ent_path = repo_dir_it->path();
    if (fs::is_regular_file(ent_path)) {
      fs::path rel_path = fs::relative(ent_path, repo_dir);
      if (file_unordered_set.count(reinterpret_cast<opcuabridge::FileSetEntry>(rel_path.c_str())) == 0) {
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
