#ifndef UPTANE_IPUPTANESECONDARY_H_
#define UPTANE_IPUPTANESECONDARY_H_

#include <chrono>
#include "uptane/secondaryinterface.h"

namespace Uptane {
class IpUptaneSecondary : public SecondaryInterface {
 public:
  explicit IpUptaneSecondary(const SecondaryConfig& conf) : SecondaryInterface(conf) {}

  // SecondaryInterface implementation
  PublicKey getPublicKey() override;
  bool putMetadata(const RawMetaPack& meta_pack) override;
  int32_t getRootVersion(bool /* director */) override { return 0; }
  bool putRoot(const std::string& /* root */, bool /* director */) override { return true; }
  bool sendFirmware(const std::string& data) override;
  Json::Value getManifest() override;

  sockaddr_storage getAddr() { return sconfig.ip_addr; }
};

}  // namespace Uptane

#endif  // UPTANE_IPUPTANESECONDARY_H_
