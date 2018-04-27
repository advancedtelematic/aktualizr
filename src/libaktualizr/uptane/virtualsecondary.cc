#include "uptane/virtualsecondary.h"

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto/crypto.h"
#include "utilities/utils.h"

namespace Uptane {
VirtualSecondary::VirtualSecondary(const SecondaryConfig& sconfig_in) : ManagedSecondary(sconfig_in) {}

bool VirtualSecondary::storeFirmware(const std::string& target_name, const std::string& content) {
  Utils::writeFile(sconfig.target_name_path, target_name);
  Utils::writeFile(sconfig.firmware_path, content);
  sync();
  return true;
}

bool VirtualSecondary::getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) {
  std::string content;

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
