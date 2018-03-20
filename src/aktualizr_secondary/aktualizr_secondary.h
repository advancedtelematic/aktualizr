#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_ipc.h"
#include "channel.h"
#include "ipuptaneconnection.h"
#include "types.h"
#include "uptane/tuf.h"
#include "utils.h"

class AktualizrSecondary {
 public:
  AktualizrSecondary(const AktualizrSecondaryConfig& config);
  void run();
  void stop();

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
  AktualizrSecondaryConfig config_;
  IpUptaneConnection conn_;
};

#endif  // AKTUALIZR_SECONDARY_H
