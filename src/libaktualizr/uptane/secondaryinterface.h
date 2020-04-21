#ifndef UPTANE_SECONDARYINTERFACE_H
#define UPTANE_SECONDARYINTERFACE_H

#include <functional>
#include <memory>
#include <string>

#include "json/json.h"
#include "uptane/manifest.h"
#include "uptane/tuf.h"

class StorageTargetRHandle;

using ImageReaderProvider = std::function<std::unique_ptr<StorageTargetRHandle>(const Uptane::Target& target)>;
using TlsCredsProvider = std::function<std::string()>;

namespace Uptane {

class SecondaryInterface {
 public:
  using Ptr = std::shared_ptr<SecondaryInterface>;

 public:
  virtual std::string Type() const = 0;
  virtual EcuSerial getSerial() const = 0;
  virtual Uptane::HardwareIdentifier getHwId() const = 0;
  virtual PublicKey getPublicKey() const = 0;

  virtual Uptane::Manifest getManifest() const = 0;
  virtual bool putMetadata(const RawMetaPack& meta_pack) = 0;
  virtual bool ping() const = 0;

  virtual int32_t getRootVersion(bool director) const = 0;
  virtual bool putRoot(const std::string& root, bool director) = 0;

  virtual data::ResultCode::Numeric sendFirmware(const Target& target) = 0;
  virtual data::ResultCode::Numeric install(const Target& target) = 0;

  virtual ~SecondaryInterface() = default;
};

}  // namespace Uptane

#endif  // UPTANE_SECONDARYINTERFACE_H
