#ifndef UPTANE_SECONDARYINTERFACE_H
#define UPTANE_SECONDARYINTERFACE_H

#include <future>
#include <string>

#include "json/json.h"

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
  // This ctor should be removed as the secondary configuration SecondaryConfig
  // is the secondaries's specific, see SecondaryConfig declaration
  // explicit SecondaryInterface(SecondaryConfig sconfig_in) : sconfig(std::move(sconfig_in)) {}
  virtual ~SecondaryInterface() = default;
  // not clear what this method for, can be removed
  // virtual void Initialize(){};  // optional step, called after device registration
  // should be pure virtual, since the current implementation reads from the secondaries specific config
  // virtual EcuSerial getSerial() { return Uptane::EcuSerial(sconfig.ecu_serial); }
  virtual EcuSerial getSerial() = 0;
  // should be pure virtual, since the current implementation reads from the secondaries specific config
  //  virtual Uptane::HardwareIdentifier getHwId() { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  virtual Uptane::HardwareIdentifier getHwId() = 0;
  virtual PublicKey getPublicKey() = 0;

  // getSerial(), getHwId() and getPublicKey() can be moved to seperate interface
  // since their usage pattern differ from the following methods' one
  virtual Json::Value getManifest() = 0;
  virtual bool putMetadata(const RawMetaPack& meta_pack) = 0;
  virtual int32_t getRootVersion(bool director) = 0;
  virtual bool putRoot(const std::string& root, bool director) = 0;

  // FIXME: Instead of std::string we should use StorageTargetRHandle
  virtual bool sendFirmware(const std::shared_ptr<std::string>& data) = 0;
  // Should be removes as it's secondary specific
  // const SecondaryConfig sconfig;

  // protected:
  // SecondaryInterface() : sconfig{} {};
};
}  // namespace Uptane

#endif  // UPTANE_SECONDARYINTERFACE_H
