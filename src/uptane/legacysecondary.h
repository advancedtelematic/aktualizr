#ifndef UPTANE_LEGACYSECONDARY_H_
#define UPTANE_LEGACYSECONDARY_H_

#include <string>

#include "uptane/managedsecondary.h"
#include "utilities/types.h"

namespace Uptane {
class LegacySecondary : public ManagedSecondary {
 public:
  LegacySecondary(const SecondaryConfig& sconfig_in);

 private:
  bool storeFirmware(const std::string& target_name, const std::string& content) override;
  bool getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) override;
};
}  // namespace Uptane

#endif  // UPTANE_LEGACYSECONDARY_H_
