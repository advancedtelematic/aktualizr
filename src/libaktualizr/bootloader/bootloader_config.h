#ifndef BOOTLOADER_CONFIG_H_
#define BOOTLOADER_CONFIG_H_

#include <boost/property_tree/ini_parser.hpp>
#include <ostream>

enum class RollbackMode { BootloaderNone = 0, UbootGeneric, UbootMasked };

struct BootloaderConfig {
  RollbackMode rollback_mode{RollbackMode::BootloaderNone};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

#endif  // BOOTLOADER_CONFIG_H_
