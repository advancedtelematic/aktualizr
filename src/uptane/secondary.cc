#include <logger.h>
#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto.h"
#include "secondary.h"

namespace Uptane {
Secondary::Secondary(const SecondaryConfig &config_in, Uptane::Repository *primary)
    : config(config_in), transport(primary) {
  boost::filesystem::create_directories(config.firmware_path.parent_path());
}

Json::Value Secondary::genAndSendManifest(Json::Value custom) {
  Json::Value manifest;

  // package manager will generate this part in future
  Json::Value installed_image;
  installed_image["filepath"] = config.firmware_path.string();
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
  manifest["attacks_detected"] = "";
  manifest["installed_image"] = installed_image;
  manifest["ecu_serial"] = config.ecu_serial;
  manifest["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  manifest["timeserver_time"] = "1970-01-01T00:00:00Z";

  std::string public_key = Utils::readFile((config.full_client_dir / config.ecu_public_key).string());
  std::string private_key = Utils::readFile((config.full_client_dir / config.ecu_private_key).string());
  Json::Value signed_ecu_version = Crypto::signTuf(private_key, public_key, manifest);
  return signed_ecu_version;
}

Json::Value Secondary::newTargetsCallBack(const std::vector<Uptane::Target> &targets) {
  LOGGER_LOG(LVL_trace, "I am " << config.ecu_serial << " and just got a new targets");
  std::vector<Uptane::Target>::const_iterator it;
  for (it = targets.begin(); it != targets.end(); ++it) {
    if (it->IsForSecondary(config.ecu_serial)) {
      return genAndSendManifest(data::OperationResult::fromOutcome(it->filename(), install(*it)).toJson());
    } else {
      LOGGER_LOG(LVL_trace, "Target " << *it << " isn't for this ECU");
    }
  }
  return Json::Value(Json::nullValue);
}

void Secondary::setKeys(const std::string &public_key, const std::string &private_key) {
  if (!boost::filesystem::exists(config.full_client_dir)) {
    boost::filesystem::create_directories(config.full_client_dir);
  }
  Utils::writeFile((config.full_client_dir / config.ecu_private_key).string(), private_key);
  Utils::writeFile((config.full_client_dir / config.ecu_public_key).string(), public_key);
}

bool Secondary::getPublicKey(std::string *key) {
  if (!boost::filesystem::exists(config.full_client_dir / config.ecu_public_key)) {
    return false;
  }
  *key = Utils::readFile((config.full_client_dir / config.ecu_public_key).string());
  return true;
}

data::InstallOutcome Secondary::install(const Uptane::Target &target) {
  std::string image_path = transport.getImage(target);
  boost::filesystem::copy_file(image_path, config.firmware_path.string(),
                               boost::filesystem::copy_option::overwrite_if_exists);
  return data::InstallOutcome(data::OK, "Installation successful");
}
}
