#ifndef UPTANE_ISOTPSECONDARY_H_
#define UPTANE_ISOTPSECONDARY_H_

#include "isotp_conn/isotp_conn.h"
#include "secondaryinterface.h"

namespace Uptane {

class IsoTpSecondary : public SecondaryInterface {
 public:
  explicit IsoTpSecondary(const std::string& can_iface, uint16_t can_id);

  EcuSerial getSerial() override;
  HardwareIdentifier getHwId() override;
  PublicKey getPublicKey() override;
  bool putMetadata(const RawMetaPack& meta_pack) override;
  int getRootVersion(bool director) override;
  bool putRoot(const std::string& root, bool director) override;
  bool sendFirmware(const std::shared_ptr<std::string>& data) override;
  Json::Value getManifest() override;

 private:
  IsoTpSendRecv conn;
};
}  // namespace Uptane
#endif  // UPTANE_ISOTPSECONDARY_H_
