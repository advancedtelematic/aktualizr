#ifndef AKTUALIZR_UPTANE_MANIFEST_H
#define AKTUALIZR_UPTANE_MANIFEST_H

#include <memory>

#include "json/json.h"
#include "libaktualizr/types.h"

class KeyManager;

namespace Uptane {

class ManifestIssuer {
 public:
  using Ptr = std::shared_ptr<ManifestIssuer>;

 public:
  ManifestIssuer(std::shared_ptr<KeyManager> &key_mngr, Uptane::EcuSerial ecu_serial)
      : ecu_serial_(std::move(ecu_serial)), key_mngr_(key_mngr) {}

  static Manifest assembleManifest(const InstalledImageInfo &installed_image_info, const Uptane::EcuSerial &ecu_serial);
  static Hash generateVersionHash(const std::string &data);
  static std::string generateVersionHashStr(const std::string &data);

  Manifest sign(const Manifest &manifest, const std::string &report_counter = "") const;

  Manifest assembleManifest(const InstalledImageInfo &installed_image_info) const;
  Manifest assembleManifest(const Uptane::Target &target) const;

  Manifest assembleAndSignManifest(const InstalledImageInfo &installed_image_info) const;

 private:
  const Uptane::EcuSerial ecu_serial_;
  std::shared_ptr<KeyManager> key_mngr_;
};

}  // namespace Uptane

#endif  // AKTUALIZR_UPTANE_MANIFEST_H
