#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_metadata.h"

#include "uptane/directorrepository.h"
#include "uptane/imagerepository.h"
#include "uptane/manifest.h"

class UpdateAgent;
class INvStorage;
class KeyManager;

class AktualizrSecondary {
 public:
  using Ptr = std::shared_ptr<AktualizrSecondary>;

 public:
  AktualizrSecondary(AktualizrSecondaryConfig config, std::shared_ptr<INvStorage> storage,
                     std::shared_ptr<KeyManager> key_mngr, std::shared_ptr<UpdateAgent> update_agent);

  Uptane::EcuSerial getSerial() const;
  Uptane::HardwareIdentifier getHwId() const;
  PublicKey getPublicKey() const;

  Uptane::Manifest getManifest() const;

  bool putMetadata(const Uptane::RawMetaPack& meta_pack) { return putMetadata(Metadata(meta_pack)); }
  int32_t getRootVersion(bool director) const;
  bool putRoot(const std::string& root, bool director);
  bool sendFirmware(const std::string& firmware);
  data::ResultCode::Numeric install(const std::string& target_name);

  void completeInstall();
  bool putMetadata(const Metadata& metadata);

 private:
  bool hasPendingUpdate() { return storage_->hasPendingInstall(); }
  bool doFullVerification(const Metadata& metadata);
  void uptaneInitialize();
  void initPendingTargetIfAny();

 private:
  AktualizrSecondary(const AktualizrSecondary&) = delete;
  AktualizrSecondary(const AktualizrSecondary&&) = delete;

  AktualizrSecondary& operator=(const AktualizrSecondary&) = delete;
  AktualizrSecondary& operator=(const AktualizrSecondary&&) = delete;

 private:
  Uptane::DirectorRepository director_repo_;
  Uptane::ImageRepository image_repo_;

  Uptane::Target pending_target_{Uptane::Target::Unknown()};

  AktualizrSecondaryConfig config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<KeyManager> keys_;
  Uptane::ManifestIssuer::Ptr manifest_issuer_;

  Uptane::EcuSerial ecu_serial_{Uptane::EcuSerial::Unknown()};
  Uptane::HardwareIdentifier hardware_id_{Uptane::HardwareIdentifier::Unknown()};

  std::shared_ptr<UpdateAgent> update_agent_;
};

#endif  // AKTUALIZR_SECONDARY_H
