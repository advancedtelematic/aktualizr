#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include "bootloader_config.h"

class INvStorage;

class Bootloader {
 public:
  Bootloader(BootloaderConfig config, std::shared_ptr<INvStorage> storage);
  Bootloader(const Bootloader&) = delete;
  Bootloader& operator=(const Bootloader&) = delete;

 public:
  void setBootOK() const;
  void updateNotify() const;

  // Reboot handling (uses storage)
  //
  // Note: will only flag a reboot if it was flagged for detection with
  // `rebootFlagSet()`.
  // Also, `rebootDetected()` will continue to return true until the flag
  // has been cleared, so that users can make sure that appropriate actions
  // in reaction to the reboot have been processed.
  bool supportRebootDetection() const;
  bool rebootDetected() const;
  void rebootFlagSet();
  void rebootFlagClear();
  void reboot(bool fake_reboot = false);

 private:
  const BootloaderConfig config_;

  std::shared_ptr<INvStorage> storage_;
  boost::filesystem::path reboot_sentinel_;
  std::string reboot_command_;
  bool reboot_detect_supported_{false};
};

#endif  // BOOTLOADER_H_
