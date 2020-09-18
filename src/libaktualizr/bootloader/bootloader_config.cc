#include "libaktualizr/config.h"
#include "utilities/config_utils.h"

std::ostream& operator<<(std::ostream& os, RollbackMode mode) {
  std::string mode_s;
  switch (mode) {
    case RollbackMode::kUbootGeneric:
      mode_s = "uboot_generic";
      break;
    case RollbackMode::kUbootMasked:
      mode_s = "uboot_masked";
      break;
    case RollbackMode::kFioVB:
      mode_s = "fiovb";
      break;
    default:
      mode_s = "none";
      break;
  }
  os << '"' << mode_s << '"';
  return os;
}

template <>
inline void CopyFromConfig(RollbackMode& dest, const std::string& option_name, const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    std::string mode{StripQuotesFromStrings(value.get())};
    if (mode == "uboot_generic") {
      dest = RollbackMode::kUbootGeneric;
    } else if (mode == "uboot_masked") {
      dest = RollbackMode::kUbootMasked;
    } else if (mode == "fiovb") {
      dest = RollbackMode::kFioVB;
    } else {
      dest = RollbackMode::kBootloaderNone;
    }
  }
}

void BootloaderConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(rollback_mode, "rollback_mode", pt);
  CopyFromConfig(reboot_sentinel_dir, "reboot_sentinel_dir", pt);
  CopyFromConfig(reboot_sentinel_name, "reboot_sentinel_name", pt);
  CopyFromConfig(reboot_command, "reboot_command", pt);
}

void BootloaderConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, rollback_mode, "rollback_mode");
  writeOption(out_stream, reboot_sentinel_dir, "reboot_sentinel_dir");
  writeOption(out_stream, reboot_sentinel_name, "reboot_sentinel_name");
  writeOption(out_stream, reboot_command, "reboot_command");
}
