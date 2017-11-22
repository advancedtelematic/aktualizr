#include "uptane/managedsecondary.h"

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto.h"
#include "logger.h"

namespace Uptane {
ManagedSecondary::ManagedSecondary(const SecondaryConfig &sconfig_in) : SecondaryInterface(sconfig_in) {
  // transport(primary), wait_image(false) {
  boost::filesystem::create_directories(sconfig.metadata_path);

  // TODO: FIX
  // loadMetadata(meta_pack);
  if (!loadKeys(&public_key, &private_key)) {
    if (!Crypto::generateRSAKeyPair(&public_key, &private_key)) {
      LOGGER_LOG(LVL_error, "Could not generate rsa keys for secondary " << sconfig.ecu_serial << "@"
                                                                         << sconfig.ecu_hardware_id);
      throw std::runtime_error("Unable to initialize libsodium");
    }
    storeKeys(public_key, private_key);
  }
}

bool ManagedSecondary::putMetadata(const MetaPack &meta_pack) {
  // No verification is currently performed, we can add verification in future for testing purposes
  detected_attack = "";
  current_meta = meta_pack;
  storeMetadata(current_meta);

  expected_target_name = "";
  expected_target_hashes.clear();
  expected_target_length = 0;

  bool target_found = false;

  std::vector<Uptane::Target>::const_iterator it;
  for (it = meta_pack.director_targets.targets.begin(); it != meta_pack.director_targets.targets.end(); ++it) {
    // TODO: what about hardware ID? Also missing in Uptane::Target
    if (it->ecu_identifier() == sconfig.ecu_serial) {
      if (target_found) {
        detected_attack = "Duplicate entry for this ECU";
        break;
      }
      expected_target_name = it->filename();
      expected_target_hashes = it->hashes();
      expected_target_length = it->length();
      target_found = true;
    }
  }

  if (!target_found) detected_attack = "No update for this ECU";

  return true;
}

int ManagedSecondary::getRootVersion(const bool director) {
  if (director)
    return current_meta.director_root.version();
  else
    return current_meta.image_root.version();
}

bool ManagedSecondary::putRoot(Uptane::Root root, const bool director) {
  Uptane::Root &prev_root = (director) ? current_meta.director_root : current_meta.image_root;

  // No verification is currently performed, we can add verification in future for testing purposes
  if (root.version() == prev_root.version() + 1) {
    prev_root = root;
  } else {
    std::ostringstream out;
    out << "Tried to update root version " << prev_root.version() << " with version " << root.version();
    detected_attack = out.str();
  }

  storeMetadata(current_meta);

  return true;
}

bool ManagedSecondary::sendFirmware(const uint8_t *blob, size_t size) {
  if (expected_target_name.empty()) return true;
  if (!detected_attack.empty()) return true;

  if (size > expected_target_length) {
    detected_attack = "overflow";
    return true;
  }
  // TODO: this cast shouldn't be necessary, right?
  std::string content = std::string((const char *)blob, size);
  std::vector<Hash>::const_iterator it;
  for (it = expected_target_hashes.begin(); it != expected_target_hashes.end(); it++) {
    if (it->TypeString() == "sha256") {
      if (boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(content))) != it->HashString()) {
        detected_attack = "wrong_hash";
        return true;
      }
    } else if (it->TypeString() == "sha512") {
      if (boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(content))) != it->HashString()) {
        detected_attack = "wrong_hash";
        return true;
      }
    }
  }
  detected_attack = "";
  return storeFirmware(expected_target_name, content);
}

Json::Value ManagedSecondary::getManifest() {
  std::string hash;
  std::string targetname;
  size_t target_len;
  if (!getFirmwareInfo(&targetname, target_len, &hash)) {
    return Json::nullValue;
  }

  Json::Value manifest;

  // package manager will generate this part in future
  Json::Value installed_image;
  installed_image["filepath"] = targetname;

  installed_image["fileinfo"]["hashes"]["sha256"] = hash;
  installed_image["fileinfo"]["length"] = static_cast<Json::Int64>(target_len);

  manifest["attacks_detected"] = detected_attack;
  manifest["installed_image"] = installed_image;
  manifest["ecu_serial"] = sconfig.ecu_serial;

  Json::Value signed_ecu_version = Crypto::signTuf(NULL, private_key, public_key, manifest);
  return signed_ecu_version;
}

void ManagedSecondary::storeKeys(const std::string &public_key, const std::string &private_key) {
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_private_key).string(), private_key);
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_public_key).string(), public_key);
}

bool ManagedSecondary::loadKeys(std::string *public_key, std::string *private_key) {
  if (!boost::filesystem::exists(sconfig.ecu_public_key) || !boost::filesystem::exists(sconfig.ecu_private_key))
    return false;

  *private_key = Utils::readFile(sconfig.ecu_private_key);
  *public_key = Utils::readFile(sconfig.ecu_public_key);
  return true;
}
}
