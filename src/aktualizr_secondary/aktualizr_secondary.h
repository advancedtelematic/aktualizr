#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_common.h"
#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_interface.h"
#include "channel.h"
#include "secondary_ipc/aktualizr_secondary_ipc.h"
#include "secondary_ipc/ipuptaneconnection.h"
#include "storage/invstorage.h"
#include "uptane/tuf.h"
#include "utilities/keymanager.h"
#include "utilities/types.h"
#include "utilities/utils.h"

class AktualizrSecondary : public AktualizrSecondaryInterface, private AktualizrSecondaryCommon {
 public:
  AktualizrSecondary(const AktualizrSecondaryConfig& config, const std::shared_ptr<INvStorage>& storage);
  void run() override;
  void stop() override;

  // implementation of primary's SecondaryInterface
  std::string getSerialResp() const;
  std::string getHwIdResp() const;
  std::pair<KeyType, std::string> getPublicKeyResp() const;
  Json::Value getManifestResp() const;
  bool putMetadataResp(const Uptane::MetaPack& meta_pack);
  int32_t getRootVersionResp(bool director) const;
  bool putRootResp(Uptane::Root root, bool director);
  bool sendFirmwareResp(const std::string& firmware);
  void addPrimary(sockaddr_storage& addr, in_port_t port) {
    std::unique_lock<std::mutex> lock(primaries_mutex);

    primaries_map[addr] = port;
  }
  static void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert,
                                        std::string* pkey, std::string* treehub_server);

 private:
  IpUptaneConnection conn_;
};

#endif  // AKTUALIZR_SECONDARY_H
