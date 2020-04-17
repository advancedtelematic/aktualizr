#include "manifest.h"

#include "crypto/keymanager.h"

namespace Uptane {

std::string Manifest::filepath() const { return (*this)["signed"]["installed_image"]["filepath"].asString(); }

Hash Manifest::installedImageHash() const {
  // TODO: proper verification of the required fields
  return Hash(Hash::Type::kSha256, (*this)["signed"]["installed_image"]["fileinfo"]["hashes"]["sha256"].asString());
}

std::string Manifest::signature() const {
  // TODO: proper verification of the required fields
  return (*this)["signatures"][0]["sig"].asString();
}

std::string Manifest::signedBody() const {
  // TODO: proper verification of the required fields
  return Utils::jsonToCanonicalStr((*this)["signed"]);
}

bool Manifest::verifySignature(const PublicKey &pub_key) const {
  if (!(isMember("signatures") && isMember("signed"))) {
    LOG_ERROR << "Missing either signature or the signing body/subject: " << *this;
    return false;
  }

  return pub_key.VerifySignature(signature(), signedBody());
}

Manifest ManifestIssuer::sign(const Manifest &manifest, const std::string &report_counter) const {
  Manifest manifest_to_sign = manifest;
  if (!report_counter.empty()) {
    manifest_to_sign["report_counter"] = report_counter;
  }
  return key_mngr_->signTuf(manifest_to_sign);
}

Manifest ManifestIssuer::assembleManifest(const InstalledImageInfo &installed_image_info,
                                          const Uptane::EcuSerial &ecu_serial) {
  Json::Value installed_image;
  installed_image["filepath"] = installed_image_info.name;
  installed_image["fileinfo"]["length"] = Json::UInt64(installed_image_info.len);
  installed_image["fileinfo"]["hashes"]["sha256"] = installed_image_info.hash;

  Json::Value unsigned_ecu_version;
  unsigned_ecu_version["attacks_detected"] = "";
  unsigned_ecu_version["installed_image"] = installed_image;
  unsigned_ecu_version["ecu_serial"] = ecu_serial.ToString();
  unsigned_ecu_version["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  unsigned_ecu_version["timeserver_time"] = "1970-01-01T00:00:00Z";
  return unsigned_ecu_version;
}

Hash ManifestIssuer::generateVersionHash(const std::string &data) { return Hash::generate(Hash::Type::kSha256, data); }

std::string ManifestIssuer::generateVersionHashStr(const std::string &data) {
  // think of unifying a hash case,we use both lower and upper cases
  return boost::algorithm::to_lower_copy(generateVersionHash(data).HashString());
}

Manifest ManifestIssuer::assembleManifest(const InstalledImageInfo &installed_image_info) const {
  return assembleManifest(installed_image_info, ecu_serial_);
}

Manifest ManifestIssuer::assembleManifest(const Uptane::Target &target) const {
  return assembleManifest(target.getTargetImageInfo());
}

Manifest ManifestIssuer::assembleAndSignManifest(const InstalledImageInfo &installed_image_info) const {
  return key_mngr_->signTuf(assembleManifest(installed_image_info));
}

}  // namespace Uptane
