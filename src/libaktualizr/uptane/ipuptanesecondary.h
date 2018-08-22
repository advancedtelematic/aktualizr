#ifndef UPTANE_IPUPTANESECONDARY_H_
#define UPTANE_IPUPTANESECONDARY_H_

#include <chrono>
#include <future>

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
  bool sendFirmwareAsync(const std::shared_ptr<std::string>& data) override;
  Json::Value getManifest() override;

  sockaddr_storage getAddr() { return sconfig.ip_addr; }

 private:
  bool sendFirmware(const std::shared_ptr<std::string>& data);
  std::future<bool> install_future;
};

}  // namespace Uptane

#endif  // UPTANE_IPUPTANESECONDARY_H_
