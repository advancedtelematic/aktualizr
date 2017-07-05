#include <boost/algorithm/hex.hpp>

#include "crypto.h"
#include "secondary.h"

namespace Uptane {
Secondary::Secondary(const SecondaryConfig &config_in, Uptane::Repository *primary)
    : config(config_in), transport(primary) {}

Json::Value Secondary::genAndSendManifest() {
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

  manifest["attacks_detected"] = "";
  manifest["installed_image"] = installed_image;
  manifest["ecu_serial"] = config.ecu_serial;
  manifest["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  manifest["timeserver_time"] = "1970-01-01T00:00:00Z";
  Json::Value signed_ecu_version = Crypto::signTuf((config.full_client_dir / config.ecu_private_key).string(),
                                                   (config.full_client_dir / config.ecu_public_key).string(), manifest);
  return signed_ecu_version;
}

void Secondary::newTargetsCallBack(const std::vector<Uptane::Target> &targets) {
  std::cout << "I am " << config.ecu_serial << " and just got a new targets\n";

  std::vector<Uptane::Target>::const_iterator it;
  for (it = targets.begin(); it != targets.end(); ++it) {
    if (it->IsForSecondary(config.ecu_serial)) {
      install(*it);
      break;
    } else {
      std::cout << "Target " << *it << " isn't for this ECU\n";
    }
  }
}

void Secondary::setPrivateKey(const std::string &pkey) {
  Utils::writeFile((config.full_client_dir / config.ecu_private_key).string(), pkey);
}

void Secondary::install(const Uptane::Target &target) {
  std::string image = transport.getImage(target);
  Utils::writeFile(config.firmware_path.string(), image);
}
};
