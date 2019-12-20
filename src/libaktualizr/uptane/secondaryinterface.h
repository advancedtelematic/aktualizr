#ifndef UPTANE_SECONDARYINTERFACE_H
#define UPTANE_SECONDARYINTERFACE_H

#include <future>
#include <string>

#include "json/json.h"

#include "uptane/tuf.h"

namespace Uptane {

class SecondaryInterface {
 public:
  SecondaryInterface() = default;
  virtual ~SecondaryInterface() = default;

  SecondaryInterface(const SecondaryInterface&) = delete;
  SecondaryInterface& operator=(const SecondaryInterface&) = delete;

  // getSerial(), getHwId() and getPublicKey() can be moved to seperate interface
  // since their usage pattern differ from the following methods' one
  virtual EcuSerial getSerial() = 0;
  virtual Uptane::HardwareIdentifier getHwId() = 0;
  virtual PublicKey getPublicKey() = 0;

  virtual Json::Value getManifest() = 0;
  virtual bool putMetadata(const RawMetaPack& meta_pack) = 0;
  virtual int32_t getRootVersion(bool director) = 0;
  virtual bool putRoot(const std::string& root, bool director) = 0;

  // FIXME: Instead of std::string we should use StorageTargetRHandle
  virtual bool sendFirmware(const std::shared_ptr<std::string>& data) = 0;

  virtual data::ResultCode::Numeric install(const std::string& target_name) = 0;
};
}  // namespace Uptane

#endif  // UPTANE_SECONDARYINTERFACE_H
