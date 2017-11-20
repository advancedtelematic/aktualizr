#include "virtualsecondary.h"

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto.h"
#include "logger.h"

namespace Uptane {
VirtualSecondary::VirtualSecondary(const VirtualSecondaryConfig &config_in, Uptane::Repository *primary)
    : config(config_in), transport(primary), wait_image(false) {
  boost::filesystem::create_directories(config.firmware_path.parent_path());
}

Json::Value verifyMeta(const TimeMeta &time_meta, const Root &root_meta, const Targets &targets_meta) {
  Json::Value result;
  // No verification is currently performed
  result["valid"] = true;

  Utils::writeFile(config.previous_time_path.string(), Utils::readFile(config.time_path.string()));
  Utils::writeFile(config.time_path.string(), new_time);

  result["wait_for_target"] = false;
  wait_for_target = false;

  std::vector<Uptane::Target>::const_iterator it;
  for (it = targets_meta.targets.begin(); it != targets_meta.targets.end(); ++it) {
    if (it->IsForVirtualSecondary(config.ecu_serial)) {
      result["wait_for_target"] = true;
      wait_for_target = true;
      expected_target_name = it->filename();
      expected_target_hashes = it->hashes();
      expected_target_length = it->length();
      break;
    }
  }

  return result;
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
  Utils::writeFile(config.target_name_path.string(), content);
  return true;
}

Json::Value VirtualSecondary::genAndSendManifest(Json::Value custom) {
  Json::Value manifest;

  // package manager will generate this part in future
  Json::Value installed_image;
  installed_image["filepath"] = Utils::readFile(config.target_name_path.string());
  std::string content =
      Utils::readFile(config.firmware_path.string());  // FIXME this is bad idea to read all image to memory, we need to
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
  manifest["ecu_serial"] = config.ecu_serial;
  manifest["previous_timeserver_time"] = Utils::readFile(config.previous_time_path.string());
  manifest["timeserver_time"] = Utils::readFile(config.time_path.string());

  std::string public_key = Utils::readFile((config.full_client_dir / config.ecu_public_key).string());
  std::string private_key = Utils::readFile((config.full_client_dir / config.ecu_private_key).string());
  Json::Value signed_ecu_version = Crypto::signTuf(private_key, public_key, manifest);
  return signed_ecu_version;
}

void VirtualSecondary::setKeys(const std::string &public_key, const std::string &private_key) {
  Utils::writeFile((config.full_client_dir / config.ecu_private_key).string(), private_key);
  Utils::writeFile((config.full_client_dir / config.ecu_public_key).string(), public_key);
}

bool VirtualSecondary::getPublicKey(std::string *key) {
  if (!boost::filesystem::exists(config.ecu_public_key())) {
    return false;
  }
  *key = Utils::readFile(config.ecu_public_key());
  return true;
}
}
