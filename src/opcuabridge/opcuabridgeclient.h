#ifndef OPCUABRIDGE_CLIENT_H_
#define OPCUABRIDGE_CLIENT_H_

#include <string>

#include "opcuabridge.h"

struct UA_Client;

namespace boost {
namespace filesystem {
class path;
}
}  // namespace boost

namespace Uptane {
class SecondaryConfig;
}

namespace opcuabridge {

struct SelectEndPoint {
  explicit SelectEndPoint(const Uptane::SecondaryConfig&);

  std::string uri;
};

class Client {
 public:
  explicit Client(const SelectEndPoint&) noexcept;
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  operator bool() const;

  Configuration recvConfiguration() const;
  VersionReport recvVersionReport() const;
  bool sendMetadataFiles(std::vector<MetadataFile>&) const;
  bool syncDirectoryFiles(const boost::filesystem::path&) const;

 private:
  UA_Client* client_;
};

}  // namespace opcuabridge

#endif  // OPCUABRIDGE_CLIENT_H_
