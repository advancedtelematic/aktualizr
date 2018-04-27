#ifndef OPCUABRIDGE_CLIENT_H_
#define OPCUABRIDGE_CLIENT_H_

#include <string>

#include "opcuabridge.h"

struct UA_Client;

namespace boost {
namespace filesystem {
class path;
}  // namespace filesystem
}  // namespace boost

namespace Uptane {
class SecondaryConfig;
}  // namespace Uptane

namespace opcuabridge {

class SelectEndPoint {
 public:
  explicit SelectEndPoint(const Uptane::SecondaryConfig& /*sconfig*/);

  const std::string& getUrl() const { return url_; }

 private:
  struct DiscoveredEndPointCacheEntry {
    std::string serial;
    std::string hwid;
    std::string opcua_server_url;
  };

  struct DiscoveredEndPointCacheEntryHash {
    typedef DiscoveredEndPointCacheEntry argument_type;
    typedef std::size_t result_type;

    result_type operator()(argument_type const& e) const;
  };

  struct DiscoveredEndPointCacheEntryEqual {
    typedef DiscoveredEndPointCacheEntry argument_type;

    bool operator()(const argument_type& lhs, const argument_type& rhs) const;
  };

  typedef std::unordered_set<DiscoveredEndPointCacheEntry, DiscoveredEndPointCacheEntryHash,
                             DiscoveredEndPointCacheEntryEqual>
      DiscoveredEndPointCache;

  bool endPointConfirmed(const std::string& opcua_server_url, const Uptane::SecondaryConfig& /*sconfig*/) const;
  std::string makeOpcuaServerUri(const std::string& address) const;
  void considerLdsRegisteredEndPoints(const std::string& opcua_lds_url);

  thread_local static DiscoveredEndPointCache discovered_end_points_cache_;

  std::string url_;
};

class Client {
 public:
  explicit Client(const SelectEndPoint& /*selector*/) noexcept;
  explicit Client(const std::string& url) noexcept;
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  explicit operator bool() const;

  Configuration recvConfiguration() const;
  VersionReport recvVersionReport() const;
  OriginalManifest recvOriginalManifest() const;
  bool sendMetadataFiles(std::vector<MetadataFile>& /*files*/) const;
  bool syncDirectoryFiles(const boost::filesystem::path& /*repo_dir*/) const;

 private:
  UA_Client* client_;
};

}  // namespace opcuabridge

#endif  // OPCUABRIDGE_CLIENT_H_
