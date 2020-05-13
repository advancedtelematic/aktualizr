#ifndef PRIMARY_VIRTUALSECONDARY_H_
#define PRIMARY_VIRTUALSECONDARY_H_

#include <string>

#include "managedsecondary.h"
#include "utilities/types.h"

namespace Primary {

class VirtualSecondaryConfig : public ManagedSecondaryConfig {
 public:
  VirtualSecondaryConfig() : ManagedSecondaryConfig(Type) {}
  VirtualSecondaryConfig(const Json::Value& json_config);

  static std::vector<VirtualSecondaryConfig> create_from_file(const boost::filesystem::path& file_full_path);
  void dump(const boost::filesystem::path& file_full_path) const;

 public:
  static const char* const Type;
};

class VirtualSecondary : public ManagedSecondary {
 public:
  explicit VirtualSecondary(Primary::VirtualSecondaryConfig sconfig_in);
  ~VirtualSecondary() override = default;

  std::string Type() const override { return VirtualSecondaryConfig::Type; }
  data::InstallationResult putMetadata(const Uptane::Target& target) override;

  bool ping() const override { return true; }

 private:
  bool getFirmwareInfo(Uptane::InstalledImageInfo& firmware_info) const override;
};

}  // namespace Primary

#endif  // PRIMARY_VIRTUALSECONDARY_H_
