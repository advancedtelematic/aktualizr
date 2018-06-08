#include "bootloader.h"
#include "utilities/config_utils.h"
#include "utilities/exceptions.h"
#include "utilities/utils.h"

std::ostream& operator<<(std::ostream& os, const RollbackMode mode) {
  std::string mode_s;
  switch (mode) {
    case kUbootGeneric:
      mode_s = "uboot_generic";
      break;
    case kUbootMasked:
      mode_s = "uboot_masked";
      break;
    default:
      mode_s = "none";
      break;
  }
  os << '"' << mode_s << '"';
  return os;
}

void BootloaderConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  std::string mode_s;
  CopyFromConfig(mode_s, "rollback_mode", pt);
  if (mode_s == "uboot_generic") {
    rollback_mode = kUbootGeneric;
  } else if (mode_s == "uboot_masked") {
    rollback_mode = kUbootMasked;
  } else {
    rollback_mode = kBootloaderNone;
  }
}

void BootloaderConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, rollback_mode, "rollback_mode");
}

void Bootloader::setBootOK() const {
  std::string sink;
  switch (config_.rollback_mode) {
    case kBootloaderNone:
      break;
    case kUbootGeneric:
      if (Utils::shell("fw_setenv bootcount 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting bootcount";
      }
      break;
    case kUbootMasked:
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
    case kBootloaderNone:
      break;
    case kUbootGeneric:
      if (Utils::shell("fw_setenv bootcount 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting bootcount";
      }
      if (Utils::shell("fw_setenv rollback 0", &sink) != 0) {
        LOG_WARNING << "Failed resetting rollback flag";
      }
      break;
    case kUbootMasked:
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
