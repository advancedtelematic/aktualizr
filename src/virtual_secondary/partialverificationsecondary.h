#ifndef UPTANE_PARTIALVRIFICATIONSECONDARY_H_
#define UPTANE_PARTIALVRIFICATIONSECONDARY_H_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "libaktualizr/secondary/secondaryinterface.h"
#include "libaktualizr/types.h"
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

  void init(std::shared_ptr<SecondaryProvider> secondary_provider_in) override {
    secondary_provider_ = std::move(secondary_provider_in);
  }
  std::string Type() const override { return Primary::PartialVerificationSecondaryConfig::Type; }
  EcuSerial getSerial() const override {
    if (!sconfig.ecu_serial.empty()) {
      return Uptane::EcuSerial(sconfig.ecu_serial);
    }
    return Uptane::EcuSerial(public_key_.KeyId());
  }
  Uptane::HardwareIdentifier getHwId() const override { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  PublicKey getPublicKey() const override { return public_key_; }

  data::InstallationResult putMetadata(const Target& target) override;
  int getRootVersion(bool director) const override;
  data::InstallationResult putRoot(const std::string& root, bool director) override;

  data::InstallationResult sendFirmware(const Uptane::Target& target) override;
  data::InstallationResult install(const Uptane::Target& target) override;
  Uptane::Manifest getManifest() const override;
  bool ping() const override { return true; }

 private:
  void storeKeys(const std::string& public_key, const std::string& private_key);
  bool loadKeys(std::string* public_key, std::string* private_key);

  Primary::PartialVerificationSecondaryConfig sconfig;
  std::shared_ptr<SecondaryProvider> secondary_provider_;
  Uptane::Root root_;
  PublicKey public_key_;
  std::string private_key_;

  std::string detected_attack_;
  Uptane::Targets meta_targets_;
};
}  // namespace Uptane

#endif  // UPTANE_PARTIALVRIFICATIONSECONDARY_H_
