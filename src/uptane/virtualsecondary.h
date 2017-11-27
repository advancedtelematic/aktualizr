#ifndef UPTANE_VIRTUALSECONDARY_H_
#define UPTANE_VIRTUALSECONDARY_H_

#include <string>

#include "types.h"
#include "uptane/managedsecondary.h"

namespace Uptane {
class VirtualSecondary : public ManagedSecondary {
 public:
  VirtualSecondary(const SecondaryConfig& sconfig_in);

 private:
  virtual bool storeFirmware(const std::string& target_name, const std::string& content);
  virtual bool getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash);
};
}

#endif  // UPTANE_VIRTUALSECONDARY_H_
