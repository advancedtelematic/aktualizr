#include "uptane/managedsecondary.h"

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "logging/logging.h"
#include "utilities/crypto.h"

namespace Uptane {
ManagedSecondary::ManagedSecondary(const SecondaryConfig &sconfig_in) : SecondaryInterface(sconfig_in) {
  boost::filesystem::create_directories(sconfig.metadata_path);

  // TODO: FIX
  // loadMetadata(meta_pack);
  if (!loadKeys(&public_key, &private_key)) {
    if (!Crypto::generateKeyPair(sconfig.key_type, &public_key, &private_key)) {
      LOG_ERROR << "Could not generate rsa keys for secondary " << ManagedSecondary::getSerial() << "@"
                << sconfig.ecu_hardware_id;
      throw std::runtime_error("Unable to generate secondary rsa keys");
    }
    storeKeys(public_key, private_key);
  }
  public_key_id = Crypto::getKeyId(public_key);
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
    if (it->ecu_identifier() == getSerial()) {
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

  if (!target_found) {
    detected_attack = "No update for this ECU";
  }

  return true;
}

int ManagedSecondary::getRootVersion(const bool director) {
  if (director) {
    return current_meta.director_root.version();
  }
  return current_meta.image_root.version();
}

bool ManagedSecondary::putRoot(Uptane::Root root, const bool director) {
  Uptane::Root &prev_root = (director) ? current_meta.director_root : current_meta.image_root;

  // No verification is currently performed, we can add verification in future for testing purposes
  if (root.version() == prev_root.version() + 1) {
    prev_root = root;
  } else {
    detected_attack = "Tried to update root version " + std::to_string(prev_root.version()) + " with version " +
                      std::to_string(root.version());
  }

  storeMetadata(current_meta);

  return true;
}

bool ManagedSecondary::sendFirmware(const std::string &data) {
  if (expected_target_name.empty()) {
    return true;
  }
  if (!detected_attack.empty()) {
    return true;
  }

  if (data.size() > expected_target_length) {
    detected_attack = "overflow";
    return true;
  }

  std::vector<Hash>::const_iterator it;
  for (it = expected_target_hashes.begin(); it != expected_target_hashes.end(); it++) {
    if (it->TypeString() == "sha256") {
      if (boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(data))) !=
          boost::algorithm::to_lower_copy(it->HashString())) {
        detected_attack = "wrong_hash";
        return true;
      }
    } else if (it->TypeString() == "sha512") {
      if (boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(data))) !=
          boost::algorithm::to_lower_copy(it->HashString())) {
        detected_attack = "wrong_hash";
        return true;
      }
    }
  }
  detected_attack = "";
  return storeFirmware(expected_target_name, data);
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
  manifest["ecu_serial"] = getSerial();
  manifest["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  manifest["timeserver_time"] = "1970-01-01T00:00:00Z";

  Json::Value signed_ecu_version;

  std::string b64sig = Utils::toBase64(Crypto::RSAPSSSign(nullptr, private_key, Json::FastWriter().write(manifest)));
  Json::Value signature;
  signature["method"] = "rsassa-pss";
  signature["sig"] = b64sig;

  signature["keyid"] = public_key_id;
  signed_ecu_version["signed"] = manifest;
  signed_ecu_version["signatures"] = Json::Value(Json::arrayValue);
  signed_ecu_version["signatures"].append(signature);

  return signed_ecu_version;
}

void ManagedSecondary::storeKeys(const std::string &public_key, const std::string &private_key) {
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_private_key), private_key);
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_public_key), public_key);
}

bool ManagedSecondary::loadKeys(std::string *public_key, std::string *private_key) {
  boost::filesystem::path public_key_path = sconfig.full_client_dir / sconfig.ecu_public_key;
  boost::filesystem::path private_key_path = sconfig.full_client_dir / sconfig.ecu_private_key;

  if (!boost::filesystem::exists(public_key_path) || !boost::filesystem::exists(private_key_path)) {
    return false;
  }

  *private_key = Utils::readFile(private_key_path.string());
  *public_key = Utils::readFile(public_key_path.string());
  return true;
}
}  // namespace Uptane
