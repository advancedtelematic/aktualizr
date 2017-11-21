#ifndef UPTANE_VIRTUALSECONDARY_H_
#define UPTANE_VIRTUALSECONDARY_H_

#include <string>

#include "types.h"
#include "uptane/managedsecondary.h"

namespace Uptane {
class LegacySecondary : public ManagedSecondary {
 public:
   LegacySecondary(const SecondaryConfig &sconfig_in) : ManagedSecondary(sconfig_in);


 private:
  virtual bool storeFirmware(const std::string& target_name, const std::string& content);
  virtual bool getFirmwareInfo(std::string* targetname, size_t &target_len, std::string* sha256hash);
};
}

#endif  // UPTANE_VIRTUALSECONDARY_H_
