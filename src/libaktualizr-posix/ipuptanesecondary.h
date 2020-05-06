#ifndef UPTANE_IPUPTANESECONDARY_H_
#define UPTANE_IPUPTANESECONDARY_H_

#include <chrono>
#include <future>

#include "uptane/secondaryinterface.h"

namespace Uptane {

class IpUptaneSecondary : public SecondaryInterface {
 public:
  static SecondaryInterface::Ptr connectAndCreate(const std::string& address, unsigned short port,
                                                  ImageReader image_reader, TlsCredsProvider treehub_cred_provider);
  static SecondaryInterface::Ptr create(const std::string& address, unsigned short port, int con_fd,
                                        ImageReader image_reader, TlsCredsProvider treehub_cred_provider);

  static SecondaryInterface::Ptr connectAndCheck(const std::string& address, unsigned short port, EcuSerial serial,
                                                 HardwareIdentifier hw_id, PublicKey pub_key, ImageReader image_reader,
                                                 TlsCredsProvider treehub_cred_provider);

  explicit IpUptaneSecondary(const std::string& address, unsigned short port, EcuSerial serial,
                             HardwareIdentifier hw_id, PublicKey pub_key, ImageReader image_reader,
                             TlsCredsProvider treehub_cred_provider);

  std::string Type() const override { return "IP"; }
  EcuSerial getSerial() const override { return serial_; };
  Uptane::HardwareIdentifier getHwId() const override { return hw_id_; }
  PublicKey getPublicKey() const override { return pub_key_; }

  bool putMetadata(const RawMetaPack& meta_pack) override;
  int32_t getRootVersion(bool /* director */) const override { return 0; }
  bool putRoot(const std::string& /* root */, bool /* director */) override { return true; }
  // bool sendFirmware(const std::string& data) override;
  data::ResultCode::Numeric install(const Uptane::Target& target) override;
  Manifest getManifest() const override;
  bool ping() const override;

 private:
  const std::pair<std::string, uint16_t>& getAddr() const { return addr_; }
  data::ResultCode::Numeric install_v1(const Uptane::Target& target);
  data::ResultCode::Numeric install_v2(const Uptane::Target& target);
  bool sendFirmware(const std::string& data);
  bool sendFirmwareData(const uint8_t* data, size_t size);

 private:
  std::mutex install_mutex;

  std::pair<std::string, uint16_t> addr_;
  const EcuSerial serial_;
  const HardwareIdentifier hw_id_;
  const PublicKey pub_key_;

  ImageReader image_reader_;
  TlsCredsProvider treehub_cred_provider_;
};

}  // namespace Uptane

#endif  // UPTANE_IPUPTANESECONDARY_H_
