#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_ipc.h"
#include "channel.h"
#include "types.h"
#include "uptane/tuf.h"

struct SocketCloser {
  void operator()(int* ptr) const {
    close(*ptr);
    delete ptr;
  }
};

using SocketHandle = std::unique_ptr<int, SocketCloser>;

class AktualizrSecondary {
 public:
  AktualizrSecondary(const AktualizrSecondaryConfig& config);
  void run();
  void stop();
  int listening_port() const;

  // implementation of primary's SecondaryInterface
  std::string getSerialResp();
  std::string getHwIdResp();
  std::pair<KeyType, std::string> getPublicKeyResp();
  Json::Value getManifestResp();
  bool putMetadataResp(const Uptane::MetaPack& meta_pack);
  int32_t getRootVersionResp(bool director);
  bool putRootResp(Uptane::Root root, bool director);
  bool sendFirmwareResp(const std::string& firmware);

 private:
  void handle_connection_msgs(SocketHandle con, std::unique_ptr<sockaddr_storage> addr);
  void open_socket();
  AktualizrSecondaryConfig config_;
  SocketHandle socket_hdl_;
  SecondaryPacket::ChanType channel_;
};

#endif  // AKTUALIZR_SECONDARY_H
