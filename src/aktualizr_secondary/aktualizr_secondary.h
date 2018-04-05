#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_ipc.h"
#include "channel.h"
#include "invstorage.h"
#include "ipuptaneconnection.h"
#include "keymanager.h"
#include "packagemanagerfactory.h"
#include "packagemanagerinterface.h"
#include "types.h"
#include "uptane/tuf.h"
#include "utils.h"

class AktualizrSecondary {
 public:
  AktualizrSecondary(const AktualizrSecondaryConfig& config, const std::shared_ptr<INvStorage>& storage);
  void run();
  void stop();

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
  bool uptaneInitialize();

  AktualizrSecondaryConfig config_;
  IpUptaneConnection conn_;

  std::shared_ptr<INvStorage> storage_;
  KeyManager keys_;
  std::string ecu_serial_;
  std::string hardware_id_;
  std::shared_ptr<PackageManagerInterface> pacman;
  Uptane::Root root_;
  Uptane::Targets meta_targets_;
  std::string detected_attack_;
  std::unique_ptr<Uptane::Target> target_;
  std::mutex primaries_mutex;
  std::map<sockaddr_storage, in_port_t> primaries_map;
};

#endif  // AKTUALIZR_SECONDARY_H
