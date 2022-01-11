#ifndef PRIMARY_FIRMWARESECONDARY_H_
#define PRIMARY_FIRMWARESECONDARY_H_

#include <string>

#include "libaktualizr/types.h"
#include "managedsecondary.h"

namespace Primary {

class FirmwareFileSecondaryConfig : public ManagedSecondaryConfig {
 public:
  FirmwareFileSecondaryConfig() : ManagedSecondaryConfig(Type) {}
  explicit FirmwareFileSecondaryConfig(const Json::Value& json_config);

  static std::vector<FirmwareFileSecondaryConfig> create_from_file(const boost::filesystem::path& file_full_path);
  void dump(const boost::filesystem::path& file_full_path) const;

 public:
  static const char* const Type;
};

class FirmwareFileSecondary : public Primary::ManagedSecondary {
 public:
  explicit FirmwareFileSecondary(FirmwareFileSecondaryConfig sconfig_in);
  ~FirmwareFileSecondary() override = default;

  std::string Type() const override { return FirmwareFileSecondaryConfig::Type; }
  data::InstallationResult putMetadata(const Uptane::Target& target) override;
  data::InstallationResult putRoot(const std::string& root, bool director) override;
  data::InstallationResult sendFirmware(const Uptane::Target& target) override;
  data::InstallationResult install(const Uptane::Target& target) override;
  bool ping() const override { return true; }

 protected:
  bool getFirmwareInfo(Uptane::InstalledImageInfo& firmware_info) const override;

 private:
  uint64_t last_installed_len = 0;
  std::string last_installed_name;
  std::string last_installed_sha256;
};

}  // namespace Primary
#endif  // PRIMARY_FIRMWARESECONDARY_H_
