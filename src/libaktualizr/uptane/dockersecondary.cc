#include "uptane/dockersecondary.h"

#include "logging/logging.h"

#include <sstream>

using std::stringstream;

namespace Uptane {
DockerComposeSecondary::DockerComposeSecondary(const SecondaryConfig& sconfig_in) : ManagedSecondary(sconfig_in) {}

bool DockerComposeSecondary::storeFirmware(const std::string& target_name, const std::string& content) {
  Utils::writeFile(sconfig.target_name_path, target_name, true);
  Utils::writeFile(sconfig.firmware_path, content, true);
  stringstream cmd_line;
  cmd_line << "docker-compose -f " << sconfig.firmware_path << " up -d";
  LOG_INFO << " Running " << cmd_line.str();
  int rescode = system(cmd_line.str().c_str());
  return rescode == 0;
}

bool DockerComposeSecondary::getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) {
  std::string content;
  // Send hash of yaml

  if (!boost::filesystem::exists(sconfig.target_name_path) || !boost::filesystem::exists(sconfig.firmware_path)) {
    *target_name = std::string("noimage");
    content = "";
  } else {
    *target_name = Utils::readFile(sconfig.target_name_path.string());
    content = Utils::readFile(sconfig.firmware_path.string());
  }
  *sha256hash = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(content)));
  target_len = content.size();

  return true;
}
}  // namespace Uptane