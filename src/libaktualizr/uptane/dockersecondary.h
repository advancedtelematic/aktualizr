#ifndef UPTANE_DOCKERSECONDARY_H_
#define UPTANE_DOCKERSECONDARY_H_

#include <string>

#include "uptane/managedsecondary.h"
#include "utilities/types.h"

namespace Uptane {

/**
 * An Uptane secondary that runs on the same device as the primary but treats
 * the firmware that it is pushed as a docker-compose yaml file
 */
class DockerComposeSecondary : public ManagedSecondary {
 public:
  explicit DockerComposeSecondary(const SecondaryConfig& sconfig_in);
  ~DockerComposeSecondary() override = default;

 private:
  bool storeFirmware(const std::string& target_name, const std::string& content) override;
  bool getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) override;
};
}  // namespace Uptane

#endif  // UPTANE_DOCKERSECONDARY_H_
