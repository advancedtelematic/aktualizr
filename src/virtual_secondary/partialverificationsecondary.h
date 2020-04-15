#ifndef UPTANE_PARTIALVRIFICATIONSECONDARY_H_
#define UPTANE_PARTIALVRIFICATIONSECONDARY_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "uptane/secondaryinterface.h"
#include "utilities/types.h"

#include "managedsecondary.h"

namespace Primary {

class PartialVerificationSecondaryConfig : public ManagedSecondaryConfig {
 public:
  PartialVerificationSecondaryConfig() : ManagedSecondaryConfig(Type) {}

 public:
  constexpr static const char* const Type = "partial-verification";
};

}  // namespace Primary

namespace Uptane {

class PartialVerificationSecondary : public SecondaryInterface {
 public:
  explicit PartialVerificationSecondary(Primary::PartialVerificationSecondaryConfig sconfig_in);

  std::string Type() const override { return Primary::PartialVerificationSecondaryConfig::Type; }
  EcuSerial getSerial() const override {
    if (!sconfig.ecu_serial.empty()) {
      return Uptane::EcuSerial(sconfig.ecu_serial);
    }
    return Uptane::EcuSerial(public_key_.KeyId());
  }
  Uptane::HardwareIdentifier getHwId() const override { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  PublicKey getPublicKey() const override { return public_key_; }

  bool putMetadata(const RawMetaPack& meta) override;
  int getRootVersion(bool director) const override;
  bool putRoot(const std::string& root, bool director) override;

  data::ResultCode::Numeric install(const Uptane::Target& target_name) override;
  Uptane::Manifest getManifest() const override;
  bool ping() const override { return true; }

 private:
  void storeKeys(const std::string& public_key, const std::string& private_key);
  bool loadKeys(std::string* public_key, std::string* private_key);

  Primary::PartialVerificationSecondaryConfig sconfig;
  Uptane::Root root_;
  PublicKey public_key_;
  std::string private_key_;

  std::string detected_attack_;
  Uptane::Targets meta_targets_;
};
}  // namespace Uptane

#endif  // UPTANE_PARTIALVRIFICATIONSECONDARY_H_
