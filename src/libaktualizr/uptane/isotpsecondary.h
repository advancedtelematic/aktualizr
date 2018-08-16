#ifndef UPTANE_ISOTPSECONDARY_H_
#define UPTANE_ISOTPSECONDARY_H_

#include "isotp_conn/isotp_conn.h"
#include "secondaryinterface.h"

namespace Uptane {

class IsoTpSecondary : public SecondaryInterface {
 public:
  explicit IsoTpSecondary(const SecondaryConfig& sconfig_in);
  EcuSerial getSerial() override;
  HardwareIdentifier getHwId() override;
  PublicKey getPublicKey() override;
  bool putMetadata(const RawMetaPack& meta_pack) override;
  int getRootVersion(bool director) override;
  bool putRoot(const std::string& root, bool director) override;
  bool sendFirmwareAsync(const std::shared_ptr<std::string>& data) override;
  Json::Value getManifest() override;

 private:
  IsoTpSendRecv conn;
};
}
#endif  // UPTANE_ISOTPSECONDARY_H_
