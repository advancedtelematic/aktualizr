#include "uptane/legacysecondary.h"

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto.h"
#include "logger.h"

namespace Uptane {
LegacySecondary::LegacySecondary(const LegacySecondaryConfig &sconfig_in, Uptane::Repository *primary)
    : sconfig(sconfig_in), transport(primary), wait_image(false) {
  boost::filesystem::create_directories(sconfig.firmware_path.parent_path());
}

bool writeImage(const uint8_t *blob, size_t size) {
  if (!wait_for_target) return false;

  wait_for_target = false;
  if (size > expected_targets_length) {
    detected_attack = "overflow";
    return true;
  }
  std::string content = std::string(blob, size);
  std::vector<Hash>::const_iterator it;
  for (it = expected_target_hashes.begin(); it != expected_target_hashes.end(); it++) {
    if (it->TypeString() == "sha256") {
      if (boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(content) != it->HashString()))) {
        detected_attack = "wrong_hash";
        return true;
      }
    } else if (it->TypeString() == "sha512") {
      if (boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(content) != it->HashString()))) {
        detected_attack = "wrong_hash";
        return true;
      }
    }
  }
  detected_attack = "";
  Utils::writeFile(sconfig.target_name_path.string(), content);
  return true;
}

Json::Value LegacySecondary::genAndSendManifest(Json::Value custom) {
  Json::Value manifest;

  // package manager will generate this part in future
  Json::Value installed_image;
  installed_image["filepath"] = Utils::readFile(sconfig.target_name_path.string());
  std::string content = Utils::readFile(
      sconfig.firmware_path.string());  // FIXME this is bad idea to read all image to memory, we need to
                                        // implement progressive hash function
  installed_image["fileinfo"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(content)));
  installed_image["fileinfo"]["length"] = static_cast<Json::Int64>(content.size());
  //////////////////

  if (custom != Json::nullValue) {
    manifest["custom"] = custom;
  }
  manifest["attacks_detected"] = detected_attack;
  manifest["installed_image"] = installed_image;
  manifest["ecu_serial"] = sconfig.ecu_serial;
  manifest["previous_timeserver_time"] = Utils::readFile(sconfig.previous_time_path.string());
  manifest["timeserver_time"] = Utils::readFile(sconfig.time_path.string());

  std::string public_key = Utils::readFile((sconfig.full_client_dir / sconfig.ecu_public_key).string());
  std::string private_key = Utils::readFile((sconfig.full_client_dir / sconfig.ecu_private_key).string());
  Json::Value signed_ecu_version = Crypto::signTuf(private_key, public_key, manifest);
  return signed_ecu_version;
}

void LegacySecondary::setKeys(const std::string &public_key, const std::string &private_key) {
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_private_key).string(), private_key);
  Utils::writeFile((sconfig.full_client_dir / sconfig.ecu_public_key).string(), public_key);
}
}
