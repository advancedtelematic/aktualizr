#ifndef UPTANE_PARTIALVRIFICATIONSECONDARY_H_
#define UPTANE_PARTIALVRIFICATIONSECONDARY_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "uptane/secondaryconfig.h"
#include "uptane/secondaryinterface.h"
#include "utilities/types.h"

namespace Uptane {

class PartialVerificationSecondary : public SecondaryInterface {
 public:
  explicit PartialVerificationSecondary(SecondaryConfig sconfig_in);

  EcuSerial getSerial() override {
    if (!sconfig.ecu_serial.empty()) {
      return Uptane::EcuSerial(sconfig.ecu_serial);
    }
    return Uptane::EcuSerial(public_key_.KeyId());
  }
  Uptane::HardwareIdentifier getHwId() override { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  PublicKey getPublicKey() override { return public_key_; }

  bool putMetadata(const RawMetaPack& meta) override;
  int getRootVersion(bool director) override;
  bool putRoot(const std::string& root, bool director) override;

  bool sendFirmware(const std::shared_ptr<std::string>& data) override;
  Json::Value getManifest() override;

 private:
  void storeKeys(const std::string& public_key, const std::string& private_key);
  bool loadKeys(std::string* public_key, std::string* private_key);

  SecondaryConfig sconfig;
  Uptane::Root root_;
  PublicKey public_key_;
  std::string private_key_;

  std::string detected_attack_;
  Uptane::Targets meta_targets_;
};
}  // namespace Uptane

#endif  // UPTANE_PARTIALVRIFICATIONSECONDARY_H_
