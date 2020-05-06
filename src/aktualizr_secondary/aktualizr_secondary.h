#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include "aktualizr_secondary_config.h"
#include "aktualizr_secondary_metadata.h"
#include "msg_handler.h"

#include "uptane/directorrepository.h"
#include "uptane/imagerepository.h"
#include "uptane/manifest.h"

class UpdateAgent;
class INvStorage;
class KeyManager;

class AktualizrSecondary : public MsgDispatcher {
 public:
  using Ptr = std::shared_ptr<AktualizrSecondary>;

  virtual void initialize() = 0;
  const Uptane::EcuSerial& serial() const { return ecu_serial_; }
  const Uptane::HardwareIdentifier& hwID() const { return hardware_id_; }
  PublicKey publicKey() const;
  Uptane::Manifest getManifest() const;

  virtual bool putMetadata(const Metadata& metadata);
  virtual bool putMetadata(const Uptane::MetaBundle& meta_bundle) { return putMetadata(Metadata(meta_bundle)); }

  virtual data::ResultCode::Numeric install();
  virtual void completeInstall() = 0;

 protected:
  AktualizrSecondary(const AktualizrSecondaryConfig& config, std::shared_ptr<INvStorage> storage);

  // protected interface to be defined by child classes, i.e. a specific IP secondary type (e.g. OSTree, File, etc)
  virtual bool getInstalledImageInfo(Uptane::InstalledImageInfo& installed_image_info) const = 0;
  virtual bool isTargetSupported(const Uptane::Target& target) const = 0;
  virtual data::ResultCode::Numeric installPendingTarget(const Uptane::Target& target) = 0;
  virtual data::InstallationResult applyPendingInstall(const Uptane::Target& target) = 0;

  // protected interface to be used by child classes
  Uptane::Target& pendingTarget() { return pending_target_; }
  INvStorage& storage() { return *storage_; }
  std::shared_ptr<INvStorage>& storagePtr() { return storage_; }
  Uptane::DirectorRepository& directorRepo() { return director_repo_; }
  std::shared_ptr<KeyManager>& keyMngr() { return keys_; }

  void initPendingTargetIfAny();

 private:
  bool doFullVerification(const Metadata& metadata);
  void uptaneInitialize();
  void registerHandlers();

  // Message handlers
  ReturnCode getInfoHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  ReturnCode getManifestHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  ReturnCode putMetaHdlr(Asn1Message& in_msg, Asn1Message& out_msg);
  ReturnCode installHdlr(Asn1Message& in_msg, Asn1Message& out_msg);

  Uptane::HardwareIdentifier hardware_id_{Uptane::HardwareIdentifier::Unknown()};
  Uptane::EcuSerial ecu_serial_{Uptane::EcuSerial::Unknown()};

  AktualizrSecondaryConfig config_;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<KeyManager> keys_;

  Uptane::ManifestIssuer::Ptr manifest_issuer_;

  Uptane::DirectorRepository director_repo_;
  Uptane::ImageRepository image_repo_;
  Uptane::Target pending_target_{Uptane::Target::Unknown()};
};

#endif  // AKTUALIZR_SECONDARY_H
