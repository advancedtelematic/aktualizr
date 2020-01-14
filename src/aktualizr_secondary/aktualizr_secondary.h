#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_metadata.h"
#include "uptane/secondaryinterface.h"

#include "uptane/directorrepository.h"
#include "uptane/imagesrepository.h"
#include "uptane/manifest.h"

class UpdateAgent;
class INvStorage;
class Bootloader;
class KeyManager;

class AktualizrSecondary : public Uptane::SecondaryInterface {
 public:
  using Ptr = std::shared_ptr<AktualizrSecondary>;

 public:
  // TODO: free AktualizrSecondary from dependencies as much as possible, e.g. bootloader
  AktualizrSecondary(AktualizrSecondaryConfig config, std::shared_ptr<INvStorage> storage,
                     std::shared_ptr<Bootloader> bootloader, std::shared_ptr<KeyManager> key_mngr,
                     std::shared_ptr<UpdateAgent> update_agent);

  Uptane::EcuSerial getSerial() const override;
  Uptane::HardwareIdentifier getHwId() const override;
  PublicKey getPublicKey() const override;

  Uptane::Manifest getManifest() const override;
  bool putMetadata(const Uptane::RawMetaPack& meta_pack) override { return putMetadata(Metadata(meta_pack)); }
  int32_t getRootVersion(bool director) const override;
  bool putRoot(const std::string& root, bool director) override;
  bool sendFirmware(const std::string& firmware) override;
  data::ResultCode::Numeric install(const std::string& target_name) override;

  bool putMetadata(const Metadata& metadata);

 private:
  bool rebootDetected() { return bootloader_->rebootDetected() && storage_->hasPendingInstall(); }
  bool doFullVerification(const Metadata& metadata);
  bool uptaneInitialize();

 private:
  // Uptane verification
  Uptane::DirectorRepository director_repo_;
  Uptane::ImagesRepository image_repo_;

  // installation
  Uptane::Target pending_target_{Uptane::Target::Unknown()};

  AktualizrSecondaryConfig config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<Bootloader> bootloader_;

  std::shared_ptr<KeyManager> keys_;
  Uptane::ManifestIssuer::Ptr manifest_issuer_;

  Uptane::EcuSerial ecu_serial_{Uptane::EcuSerial::Unknown()};
  Uptane::HardwareIdentifier hardware_id_{Uptane::HardwareIdentifier::Unknown()};

  std::shared_ptr<UpdateAgent> update_agent_;
};

#endif  // AKTUALIZR_SECONDARY_H
