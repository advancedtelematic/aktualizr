#ifndef UPTANE_ISOTPSECONDARY_H_
#define UPTANE_ISOTPSECONDARY_H_

#include "isotp_conn.h"
#include "primary/secondaryinterface.h"

namespace Uptane {

class IsoTpSecondary : public SecondaryInterface {
 public:
  explicit IsoTpSecondary(const std::string& can_iface, uint16_t can_id, ImageReaderProvider image_reader_provider);

  std::string Type() const override { return "isotp"; }
  EcuSerial getSerial() const override;
  HardwareIdentifier getHwId() const override;
  PublicKey getPublicKey() const override;
  bool putMetadata(const RawMetaPack& meta_pack) override;
  int getRootVersion(bool director) const override;
  bool putRoot(const std::string& root, bool director) override;
  data::ResultCode::Numeric sendFirmware(const Target& target) override;
  data::ResultCode::Numeric install(const Target& target) override;
  Uptane::Manifest getManifest() const override;

 private:
  mutable IsoTpSendRecv conn;
  ImageReaderProvider image_reader_provider_;
};
}  // namespace Uptane
#endif  // UPTANE_ISOTPSECONDARY_H_
