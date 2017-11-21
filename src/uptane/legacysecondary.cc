#include "uptane/legacysecondary.h"

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto.h"
#include "logger.h"

namespace Uptane {
LegacySecondary::LegacySecondary(const LegacySecondaryConfig &sconfig_in) {
  boost::filesystem::create_directories(sconfig.firmware_path.parent_path());
}

bool LegacySecondary::storeFirmware(const std::string& target_name, const std::string& content) {
  // reading target hash back is not currently supported, so primary needs to save the firmware file locally
  Utils::writeFile(sconfig.target_name_path.string(), expected_target_name);
  Utils::writeFile(sconfig.firmware_path.string(), content);

  std::string output
  int rs = Utils::shell(sconfig.flasher.string() + " --hardware-identifier " + sconfig.ecu_hardware_id + " --ecu-identifier " + sconfig.ecu_serial + " --firmware " + sconfig.firmware_path.string(), &output);

  if(rs != 0)
    LOGGER_LOG(LVL_error, "Legacy external flasher failed: " << output);
  return (rs == 0);
}

bool LegacySecondary::getFirmwareInfo(std::string* targetname, size_t &target_len, std::string* sha256hash) {
  // reading target hash back is not currently supported, just use the saved file
  if (!boost::filesystem::exists(sconfig.target_name_path) || !boost::filesystem::exists(sconfig.firmware_path))
    return false;

  *targetname = Utils::readFile(sconfig.target_name_path.string());

  std::string content = Utils::readFile(sconfig.firmware_path.string());
  *sha256hash = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(content)));
  *target_len = content.size();

  return true;
}
}
