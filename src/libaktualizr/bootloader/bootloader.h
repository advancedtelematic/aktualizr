#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include "bootloader_config.h"

#include "storage/invstorage.h"

class Bootloader {
 public:
  Bootloader(const BootloaderConfig& config, INvStorage& storage);
  void setBootOK() const;
  void updateNotify() const;

  // reboot handling (uses storage)
  bool supportRebootDetection() const;
  bool rebootDetected() const;
  void rebootFlagSet();
  void rebootFlagClear();

 private:
  const BootloaderConfig& config_;

  INvStorage& storage_;
  boost::filesystem::path reboot_sentinel_;
  bool reboot_detect_supported_{false};
};

#endif  // BOOTLOADER_H_
