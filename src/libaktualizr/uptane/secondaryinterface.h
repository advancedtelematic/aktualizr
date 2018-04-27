#ifndef UPTANE_SECONDARYINTERFACE_H
#define UPTANE_SECONDARYINTERFACE_H

#include <string>

#include "json/json.h"

#include "uptane/secondaryconfig.h"
#include "uptane/tuf.h"

/* Json snippet returned by sendMetaXXX():
 * {
 *   valid = true/false,
 *   wait_for_target = true/false
 * }
 */

namespace Uptane {

class SecondaryInterface {
 public:
  explicit SecondaryInterface(const SecondaryConfig& sconfig_in) : sconfig(sconfig_in) {}
  virtual ~SecondaryInterface() = default;
  virtual std::string getSerial() { return sconfig.ecu_serial; }
  virtual std::string getHwId() { return sconfig.ecu_hardware_id; }
  virtual std::pair<KeyType, std::string> getPublicKey() = 0;

  virtual Json::Value getManifest() = 0;
  virtual bool putMetadata(const MetaPack& meta_pack) = 0;
  virtual int32_t getRootVersion(bool director) = 0;
  virtual bool putRoot(Uptane::Root root, bool director) = 0;

  virtual bool sendFirmware(const std::string& data) = 0;

  const SecondaryConfig& sconfig;
};
}  // namespace Uptane

#endif  // UPTANE_SECONDARYINTERFACE_H
