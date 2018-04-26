#include "uptane/legacysecondary.h"

#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

#include "crypto/crypto.h"
#include "logging/logging.h"
#include "utilities/utils.h"

namespace Uptane {
LegacySecondary::LegacySecondary(const SecondaryConfig& sconfig_in) : ManagedSecondary(sconfig_in) {}

bool LegacySecondary::storeFirmware(const std::string& target_name, const std::string& content) {
  // reading target hash back is not currently supported, so primary needs to save the firmware file locally
  Utils::writeFile(sconfig.target_name_path, target_name);
  Utils::writeFile(sconfig.firmware_path, content);
  sync();

  std::stringstream command;
  std::string output;
  command << sconfig.flasher.string() << " install-software --hardware-identifier " << sconfig.ecu_hardware_id
          << " --ecu-identifier " << getSerial() << " --firmware " << sconfig.firmware_path.string() << " --loglevel "
          << loggerGetSeverity();
  int rs = Utils::shell(command.str(), &output);

  if (rs != 0) {
    // TODO: Should we do anything with the return value?
    // 1 - The firmware image is invalid.
    // 2 - Installation failure. The previous firmware was not modified.
    // 3 - Installation failure. The previous firmware was partially overwritten or erased.
    LOG_ERROR << "Legacy external flasher install-software command failed: " << output;
  }
  return (rs == 0);
}

bool LegacySecondary::getFirmwareInfo(std::string* target_name, size_t& target_len, std::string* sha256hash) {
  std::string content;

  // reading target hash back is not currently supported, just use the saved file
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
