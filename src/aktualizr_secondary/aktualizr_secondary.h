#ifndef AKTUALIZR_SECONDARY_H
#define AKTUALIZR_SECONDARY_H

#include <memory>

#include "aktualizr_secondary_config.h"
#include "uptane/secondaryinterface.h"

#include "crypto/keymanager.h"
#include "storage/invstorage.h"
#include "utilities/types.h"
#include "utilities/utils.h"

#include "uptane/directorrepository.h"
#include "uptane/imagesrepository.h"
#include "uptane/tuf.h"

#include "aktualizr_secondary_metadata.h"
#include "package_manager/packagemanagerinterface.h"

class AktualizrSecondary : public Uptane::SecondaryInterface {
 public:
  using Ptr = std::shared_ptr<AktualizrSecondary>;

 public:
  AktualizrSecondary(const AktualizrSecondaryConfig& config, const std::shared_ptr<INvStorage>& storage,
                     const std::shared_ptr<KeyManager>& key_mngr,
                     const std::shared_ptr<PackageManagerInterface>& pacman);

  Uptane::EcuSerial getSerial() const override;
  Uptane::HardwareIdentifier getHwId() const override;
  PublicKey getPublicKey() const override;
  Json::Value getManifest() const override;
  bool putMetadata(const Uptane::RawMetaPack& meta_pack) override;
  int32_t getRootVersion(bool director) const override;
  bool putRoot(const std::string& root, bool director) override;
  bool sendFirmware(const std::string& firmware) override;

  static void extractCredentialsArchive(const std::string& archive, std::string* ca, std::string* cert,
                                        std::string* pkey, std::string* treehub_server);

  bool putMetadata(const Metadata& metadata);

 private:
  void connectToPrimary();
  bool doFullVerification(const Metadata& metadata);
  bool uptaneInitialize();

 private:
  AktualizrSecondaryConfig config_;
  Uptane::DirectorRepository director_repo_;
  Uptane::ImagesRepository image_repo_;

  std::shared_ptr<INvStorage> storage_;

  std::shared_ptr<KeyManager> keys_;

  Uptane::EcuSerial ecu_serial_{Uptane::EcuSerial::Unknown()};
  Uptane::HardwareIdentifier hardware_id_{Uptane::HardwareIdentifier::Unknown()};

  std::shared_ptr<PackageManagerInterface> pacman_;
};

#endif  // AKTUALIZR_SECONDARY_H
