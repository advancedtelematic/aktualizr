#include "bootloader.h"

#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>

#include "storage/invstorage.h"
#include "utilities/exceptions.h"
#include "utilities/utils.h"

Bootloader::Bootloader(BootloaderConfig config, INvStorage& storage) : config_(std::move(config)), storage_(storage) {
  reboot_sentinel_ = config_.reboot_sentinel_dir / config_.reboot_sentinel_name;
  reboot_command_ = config_.reboot_command;

  if (!Utils::createSecureDirectory(config_.reboot_sentinel_dir)) {
    LOG_WARNING << "Could not create " << config_.reboot_sentinel_dir << " securely, reboot detection support disabled";
    reboot_detect_supported_ = false;
    return;
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

  bool sentinel_exists = boost::filesystem::exists(reboot_sentinel_);
  bool need_reboot = false;

  storage_.loadNeedReboot(&need_reboot);

  return need_reboot && !sentinel_exists;
}

void Bootloader::rebootFlagSet() {
  if (!reboot_detect_supported_) {
    return;
  }

  // set in storage + volatile flag

  Utils::writeFile(reboot_sentinel_, std::string(), false);  // empty file
  storage_.storeNeedReboot();
}

void Bootloader::rebootFlagClear() {
  if (!reboot_detect_supported_) {
    return;
  }

  // clear in storage + volatile flag

  storage_.clearNeedReboot();
  boost::filesystem::remove(reboot_sentinel_);
}

void Bootloader::reboot(bool fake_reboot) {
  if (fake_reboot) {
    boost::filesystem::remove(reboot_sentinel_);
    return;
  }
  if (setuid(0) != 0) {
    LOG_ERROR << "Failed to set/verify a root user so cannot reboot system programmatically";
    return;
  }
  sync();
  if (system(reboot_command_.c_str()) != 0) {
    LOG_ERROR << "Failed to execute the reboot command: " << reboot_command_;
  }
}
