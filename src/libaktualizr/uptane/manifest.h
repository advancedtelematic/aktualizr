#ifndef AKTUALIZR_UPTANE_MANIFEST_H
#define AKTUALIZR_UPTANE_MANIFEST_H

#include "json/json.h"
#include "tuf.h"

#include <memory>

class KeyManager;

namespace Uptane {

class Manifest : public Json::Value {
 public:
  Manifest(const Json::Value &value = Json::Value()) : Json::Value(value) {}

 public:
  std::string filepath() const;
  Hash installedImageHash() const;
  std::string signature() const;
  std::string signedBody() const;
  bool verifySignature(const PublicKey &pub_key) const;
};

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
