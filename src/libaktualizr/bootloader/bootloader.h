#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include <boost/property_tree/ini_parser.hpp>
#include <iostream>

enum RollbackMode { kBootloaderNone = 0, kUbootGeneric, kUbootMasked };

struct BootloaderConfig {
  RollbackMode rollback_mode{kBootloaderNone};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

class Bootloader {
 public:
  Bootloader(const BootloaderConfig& config) : config_{config} {}
  void setBootOK() const;
  void updateNotify() const;

 private:
  const BootloaderConfig& config_;
};

#endif  // BOOTLOADER_H_
