#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include "bootloader_config.h"

class Bootloader {
 public:
  Bootloader(const BootloaderConfig& config) : config_{config} {}
  void setBootOK() const;
  void updateNotify() const;

 private:
  const BootloaderConfig& config_;
};

#endif  // BOOTLOADER_H_
