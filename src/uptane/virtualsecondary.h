#ifndef UPTANE_VIRTUALSECONDARY_H_
#define UPTANE_VIRTUALSECONDARY_H_

#include <string>

#include "uptane/managedsecondary.h"
#include "utilities/types.h"

namespace Uptane {
class VirtualSecondary : public ManagedSecondary {
 public:
  VirtualSecondary(const SecondaryConfig& sconfig_in);

 private:
  bool storeFirmware(const std::string& target_name, const std::string& content) override;
  bool getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) override;
};
}  // namespace Uptane

#endif  // UPTANE_VIRTUALSECONDARY_H_
