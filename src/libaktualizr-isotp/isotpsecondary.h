#ifndef UPTANE_ISOTPSECONDARY_H_
#define UPTANE_ISOTPSECONDARY_H_

#include "isotp_conn.h"
#include "libaktualizr/secondary/secondaryinterface.h"

namespace Uptane {

class IsoTpSecondary : public SecondaryInterface {
 public:
  explicit IsoTpSecondary(const std::string& can_iface, uint16_t can_id);

  void init(std::shared_ptr<SecondaryProvider> secondary_provider_in) override {
    secondary_provider_ = std::move(secondary_provider_in);
  }
  std::string Type() const override { return "isotp"; }
  EcuSerial getSerial() const override;
  HardwareIdentifier getHwId() const override;
  PublicKey getPublicKey() const override;
  int getRootVersion(bool director) const override;
  data::InstallationResult putRoot(const std::string& root, bool director) override;
  data::InstallationResult putMetadata(const Target& target) override;
  data::InstallationResult sendFirmware(const Target& target) override;
  data::InstallationResult install(const Target& target) override;
  Uptane::Manifest getManifest() const override;

 private:
  std::shared_ptr<SecondaryProvider> secondary_provider_;
  mutable IsoTpSendRecv conn;
};
}  // namespace Uptane
#endif  // UPTANE_ISOTPSECONDARY_H_
