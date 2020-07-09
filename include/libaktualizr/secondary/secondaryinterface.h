#ifndef UPTANE_SECONDARYINTERFACE_H
#define UPTANE_SECONDARYINTERFACE_H

#include <string>

#include "primary/secondary_provider.h"
#include "uptane/manifest.h"
#include "uptane/tuf.h"

class SecondaryInterface {
 public:
  using Ptr = std::shared_ptr<SecondaryInterface>;

  virtual void init(std::shared_ptr<SecondaryProvider> secondary_provider_in) = 0;
  virtual std::string Type() const = 0;
  virtual Uptane::EcuSerial getSerial() const = 0;
  virtual Uptane::HardwareIdentifier getHwId() const = 0;
  virtual PublicKey getPublicKey() const = 0;

  virtual Uptane::Manifest getManifest() const = 0;
  virtual data::InstallationResult putMetadata(const Uptane::Target& target) = 0;
  virtual bool ping() const = 0;

  virtual int32_t getRootVersion(bool director) const = 0;
  virtual data::InstallationResult putRoot(const std::string& root, bool director) = 0;

  virtual data::InstallationResult sendFirmware(const Uptane::Target& target) = 0;
  virtual data::InstallationResult install(const Uptane::Target& target) = 0;

  virtual ~SecondaryInterface() = default;
};

#endif  // UPTANE_SECONDARYINTERFACE_H
