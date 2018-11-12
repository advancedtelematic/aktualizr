#include <fcntl.h>
#include <sys/stat.h>

#include <boost/filesystem.hpp>

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

static const boost::filesystem::path reboot_beacon_dir = "/var/run/aktualizr-session";
static const boost::filesystem::path reboot_beacon_filename = "need_reboot";

Bootloader::Bootloader(const BootloaderConfig& config, INvStorage& storage) : config_(config), storage_(storage) {
  if (mkdir(reboot_beacon_dir.c_str(), S_IRWXU) == -1) {
    struct stat st {};
    stat(reboot_beacon_dir.c_str(), &st);
    if (((st.st_mode & S_IFDIR) == 0) || (st.st_mode & (S_IRGRP | S_IROTH | S_IWGRP | S_IWOTH)) != 0) {
      LOG_WARNING << "Could not create " << reboot_beacon_dir << " securely, reboot detection support disabled";
      reboot_detect_supported_ = false;
      return;
    }
    // mdkir failed but directory is here with good permissions: someone created it for us
  }
  reboot_detect_supported_ = true;
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

bool Bootloader::supportRebootDetection() const { return reboot_detect_supported_; }

bool Bootloader::rebootDetected() const {
  if (!reboot_detect_supported_) {
    return false;
  }

  // true if set in storage and no volatile flag

  bool beacon_exists = boost::filesystem::exists(reboot_beacon_dir / reboot_beacon_filename);
  bool need_reboot = false;

  storage_.loadNeedReboot(&need_reboot);

  return need_reboot && !beacon_exists;
}

void Bootloader::rebootFlagSet() {
  if (!reboot_detect_supported_) {
    return;
  }

  // set in storage + volatile flag

  Utils::writeFile(reboot_beacon_dir / reboot_beacon_filename, std::string(), false);  // empty file
  storage_.storeNeedReboot();
}

void Bootloader::rebootFlagClear() {
  if (!reboot_detect_supported_) {
    return;
  }

  // clear in storage + volatile flag

  storage_.clearNeedReboot();
  boost::filesystem::remove(reboot_beacon_dir / reboot_beacon_filename);
}
