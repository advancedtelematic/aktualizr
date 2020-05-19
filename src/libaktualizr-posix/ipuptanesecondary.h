#ifndef UPTANE_IPUPTANESECONDARY_H_
#define UPTANE_IPUPTANESECONDARY_H_

#include <mutex>

#include "asn1/asn1_message.h"
#include "der_encoder.h"
#include "primary/secondaryinterface.h"

namespace Uptane {

class IpUptaneSecondary : public SecondaryInterface {
 public:
  static SecondaryInterface::Ptr connectAndCreate(const std::string& address, unsigned short port);
  static SecondaryInterface::Ptr create(const std::string& address, unsigned short port, int con_fd);

  static SecondaryInterface::Ptr connectAndCheck(const std::string& address, unsigned short port, EcuSerial serial,
                                                 HardwareIdentifier hw_id, PublicKey pub_key);

  explicit IpUptaneSecondary(const std::string& address, unsigned short port, EcuSerial serial,
                             HardwareIdentifier hw_id, PublicKey pub_key);

  std::string Type() const override { return "IP"; }
  EcuSerial getSerial() const override { return serial_; };
  Uptane::HardwareIdentifier getHwId() const override { return hw_id_; }
  PublicKey getPublicKey() const override { return pub_key_; }

  void init(std::shared_ptr<SecondaryProvider> secondary_provider_in) override {
    secondary_provider_ = std::move(secondary_provider_in);
  }
  data::InstallationResult putMetadata(const Target& target) override;
  int32_t getRootVersion(bool /* director */) const override { return 0; }
  // TODO(OTA-4552): Support multiple Root rotations.
  data::InstallationResult putRoot(const std::string& /* root */, bool /* director */) override {
    return data::InstallationResult(data::ResultCode::Numeric::kOk, "");
  }
  Manifest getManifest() const override;
  bool ping() const override;
  data::InstallationResult sendFirmware(const Uptane::Target& target) override;
  data::InstallationResult install(const Uptane::Target& target) override;

 private:
  const std::pair<std::string, uint16_t>& getAddr() const { return addr_; }
  data::InstallationResult putMetadata_v1(const Uptane::MetaBundle& meta_bundle);
  data::InstallationResult putMetadata_v2(const Uptane::MetaBundle& meta_bundle);
  data::InstallationResult sendFirmware_v1(const Uptane::Target& target);
  data::InstallationResult sendFirmware_v2(const Uptane::Target& target);
  data::InstallationResult install_v1(const Uptane::Target& target);
  data::InstallationResult install_v2(const Uptane::Target& target);
  void addMetadata(const Uptane::MetaBundle& meta_bundle, Uptane::RepositoryType repo, const Uptane::Role& role,
                   AKMetaCollection_t& collection);
  data::InstallationResult invokeInstallOnSecondary(const Uptane::Target& target);
  data::InstallationResult downloadOstreeRev(const Uptane::Target& target);
  data::InstallationResult uploadFirmware(const Uptane::Target& target);
  data::InstallationResult uploadFirmwareData(const uint8_t* data, size_t size);

  std::shared_ptr<SecondaryProvider> secondary_provider_;
  std::mutex install_mutex;
  std::pair<std::string, uint16_t> addr_;
  const EcuSerial serial_;
  const HardwareIdentifier hw_id_;
  const PublicKey pub_key_;
  uint32_t protocol_version{2};
};

}  // namespace Uptane

#endif  // UPTANE_IPUPTANESECONDARY_H_
