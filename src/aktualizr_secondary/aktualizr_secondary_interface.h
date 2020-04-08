#ifndef AKTUALIZR_SECONDARY_INTERFACE_H
#define AKTUALIZR_SECONDARY_INTERFACE_H

#include "uptane/manifest.h"

class IAktualizrSecondary {
 public:
  using Ptr = std::shared_ptr<IAktualizrSecondary>;

 public:
  virtual ~IAktualizrSecondary() = default;

  virtual std::tuple<Uptane::EcuSerial, Uptane::HardwareIdentifier, PublicKey> getInfo() const = 0;
  virtual Uptane::Manifest getManifest() const = 0;
  virtual bool putMetadata(const Uptane::RawMetaPack& meta_pack) = 0;
  virtual bool sendFirmware(const std::string& firmware) = 0;
  virtual data::ResultCode::Numeric install(const std::string& target_name) = 0;

 public:
  IAktualizrSecondary(const IAktualizrSecondary&) = delete;
  IAktualizrSecondary(const IAktualizrSecondary&&) = delete;
  IAktualizrSecondary& operator=(const IAktualizrSecondary&) = delete;
  IAktualizrSecondary& operator=(const IAktualizrSecondary&&) = delete;

 protected:
  IAktualizrSecondary() = default;
};

#endif  // AKTUALIZR_SECONDARY_INTERFACE_H
