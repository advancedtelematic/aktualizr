#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_ipc.h"
#include "channel.h"
#include "invstorage.h"
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
  AktualizrSecondary(const AktualizrSecondaryConfig& config, const boost::shared_ptr<INvStorage>& storage);
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

 private:
  bool uptaneInitialize();

  AktualizrSecondaryConfig config_;
  IpUptaneConnection conn_;

  boost::shared_ptr<INvStorage> storage_;
  std::string ecu_serial_;
  std::string hardware_id_;

  boost::shared_ptr<PackageManagerInterface> pacman;
  KeyManager keys_;
  std::string treehub_server_;

  Uptane::Root root_;
  Uptane::Targets meta_targets_;
  std::unique_ptr<Uptane::Target> target_;
  std::string detected_attack_;
};

#endif  // AKTUALIZR_SECONDARY_H
