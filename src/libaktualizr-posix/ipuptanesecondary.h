#ifndef UPTANE_IPUPTANESECONDARY_H_
#define UPTANE_IPUPTANESECONDARY_H_

#include <chrono>
#include <future>

#include "uptane/secondaryinterface.h"

namespace Uptane {

class IpUptaneSecondary : public SecondaryInterface {
 public:
  static std::pair<bool, std::shared_ptr<Uptane::SecondaryInterface>> connectAndCreate(const std::string& address,
                                                                                       unsigned short port);

  static std::pair<bool, std::shared_ptr<Uptane::SecondaryInterface>> create(const std::string& address,
                                                                             unsigned short port, int con_fd);

  explicit IpUptaneSecondary(const std::string& address, unsigned short port, EcuSerial serial,
                             HardwareIdentifier hw_id, PublicKey pub_key);

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
  const std::pair<std::string, uint16_t>& getAddr() const { return addr_; }

 private:
  std::mutex install_mutex;

  std::pair<std::string, uint16_t> addr_;
  const EcuSerial serial_;
  const HardwareIdentifier hw_id_;
  const PublicKey pub_key_;
};

}  // namespace Uptane

#endif  // UPTANE_IPUPTANESECONDARY_H_
