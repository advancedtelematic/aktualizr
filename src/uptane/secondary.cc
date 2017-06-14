#include "secondary.h"
#include "crypto.h"

static const std::string IMAGE_PATH = "/secondary_firmware.txt";
namespace Uptane {
Secondary::Secondary(const SecondaryConfig &config_in, Uptane::Repository *primary)
    : config(config_in), transport(primary) {
  if (!boost::filesystem::exists(config.full_client_dir / config.ecu_private_key)) {
    Crypto::generateRSAKeyPair((config.full_client_dir / config.ecu_public_key).string(),
                               (config.full_client_dir / config.ecu_private_key).string());
  }
}
Json::Value Secondary::genAndSendManifest() {
  Json::Value manifest;

  // package manager will generate this part in future
  Json::Value installed_image;
  installed_image["filepath"] = IMAGE_PATH;
  std::string content = Utils::readFile(IMAGE_PATH);  // FIXME this is bad idea to read all image to memory, we need to
                                                      // implement progressive hash function
  installed_image["fileinfo"]["hashes"]["sha512"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(content)));
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
    if (it->custom_["ecuIdentifier"].asString() == config.ecu_serial) {
      if (it->length_) {
        std::cout << "Good, it is my target\n";
        install(*it);
      } else {
        std::cout << "Size of target is zero, maybe it is an ostree package\n";
      }
      break;
    }
  }
}

void Secondary::install(const Uptane::Target &target) {
  std::string image = transport.getImage(target);
  Utils::writeFile(IMAGE_PATH, image);
}
};