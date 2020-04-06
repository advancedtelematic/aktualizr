#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_metadata.h"

#include "aktualizr_secondary_interface.h"
#include "msg_dispatcher.h"

#include "uptane/directorrepository.h"
#include "uptane/imagerepository.h"

class UpdateAgent;
class INvStorage;
class KeyManager;

class AktualizrSecondary : public IAktualizrSecondary {
 public:
  using Ptr = std::shared_ptr<AktualizrSecondary>;

 public:
  AktualizrSecondary(AktualizrSecondaryConfig config, std::shared_ptr<INvStorage> storage,
                     std::shared_ptr<KeyManager> key_mngr, std::shared_ptr<UpdateAgent> update_agent);

  std::tuple<Uptane::EcuSerial, Uptane::HardwareIdentifier, PublicKey> getInfo() const override;
  Uptane::Manifest getManifest() const override;
  bool putMetadata(const Uptane::RawMetaPack& meta_pack) override { return putMetadata(Metadata(meta_pack)); }
  bool sendFirmware(const std::string& firmware) override;
  data::ResultCode::Numeric sendFirmware(const uint8_t* data, size_t size) override;

  data::ResultCode::Numeric install(const std::string& target_name) override;

  MsgDispatcher& getDispatcher() { return dispatcher_; }

  Uptane::EcuSerial getSerial() const;
  Uptane::HardwareIdentifier getHwId() const;
  PublicKey getPublicKey() const;
  bool putMetadata(const Metadata& metadata);
  void completeInstall();

 private:
  bool hasPendingUpdate() { return storage_->hasPendingInstall(); }
  bool doFullVerification(const Metadata& metadata);
  void uptaneInitialize();
  void initPendingTargetIfAny();

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

  AktualizrSecondaryMsgDispatcher dispatcher_;
};

#endif  // AKTUALIZR_SECONDARY_H
