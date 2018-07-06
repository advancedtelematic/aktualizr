#include "bootloader.h"
#include "utilities/config_utils.h"
#include "utilities/exceptions.h"
#include "utilities/utils.h"

std::ostream& operator<<(std::ostream& os, const RollbackMode mode) {
  std::string mode_s;
  switch (mode) {
    case RollbackMode::kUbootGeneric:
      mode_s = "uboot_generic";
      break;
    case RollbackMode::kUbootMasked:
      mode_s = "uboot_masked";
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
    } else {
      dest = RollbackMode::kBootloaderNone;
    }
  }
}

void BootloaderConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(rollback_mode, "rollback_mode", pt);
}

void BootloaderConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, rollback_mode, "rollback_mode");
}

void Bootloader::setBootOK() const {
  std::string sink;
  switch (config_.rollback_mode) {
    case RollbackMode::kBootloaderNone:
      break;
    case RollbackMode::kUbootGeneric:
      if (Utils::shell("fw_setenv bootcount 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting bootcount";
      }
      break;
    case RollbackMode::kUbootMasked:
      if (Utils::shell("fw_setenv bootcount 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting bootcount";
      }
      if (Utils::shell("fw_setenv upgrade_available 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting upgrade_available for u-boot";
      }
      break;
    default:
      throw NotImplementedException();
  }
}

void Bootloader::updateNotify() const {
  std::string sink;
  switch (config_.rollback_mode) {
    case RollbackMode::kBootloaderNone:
      break;
    case RollbackMode::kUbootGeneric:
      if (Utils::shell("fw_setenv bootcount 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting bootcount";
      }
      if (Utils::shell("fw_setenv rollback 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting rollback flag";
      }
      break;
    case RollbackMode::kUbootMasked:
      if (Utils::shell("fw_setenv bootcount 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting bootcount";
      }
      if (Utils::shell("fw_setenv upgrade_available 1", &sink) != 0) {
        LOG_WARNING << "Failed setting upgrade_available for u-boot";
      }
      if (Utils::shell("fw_setenv rollback 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting rollback flag";
      }
      break;
    default:
      throw NotImplementedException();
  }
}
