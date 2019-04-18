#ifndef UPTANE_IPUPTANESECONDARY_H_
#define UPTANE_IPUPTANESECONDARY_H_

#include <chrono>
#include <future>

#include "uptane/secondaryinterface.h"

namespace Uptane {

class IpUptaneSecondary : public SecondaryInterface {
 public:
  static std::shared_ptr<Uptane::SecondaryInterface> create(const std::string& address, unsigned short port);

  explicit IpUptaneSecondary(const sockaddr_in& sock_addr, EcuSerial serial, HardwareIdentifier hw_id,
                             PublicKey pub_key);

  // what this method for ? Looks like should be removed out of SecondaryInterface
  void Initialize() override{};

  // It looks more natural to return const EcuSerial& and const Uptane::HardwareIdentifier&
  // and they should be 'const' methods
  EcuSerial getSerial() /*const*/ override { return serial_; };
  Uptane::HardwareIdentifier getHwId() /*const*/ override { return hw_id_; }
  PublicKey getPublicKey() /*const*/ override { return pub_key_; }

  bool putMetadata(const RawMetaPack& meta_pack) override;
  int32_t getRootVersion(bool /* director */) override { return 0; }
  bool putRoot(const std::string& /* root */, bool /* director */) override { return true; }
  bool sendFirmware(const std::shared_ptr<std::string>& data) override;
  Json::Value getManifest() override;

 private:
  const sockaddr_in& getAddr() const { return sock_address_; }

 private:
  std::mutex install_mutex;

  struct sockaddr_in sock_address_;
  const EcuSerial serial_;
  const HardwareIdentifier hw_id_;
  const PublicKey pub_key_;
};

}  // namespace Uptane

#endif  // UPTANE_IPUPTANESECONDARY_H_
