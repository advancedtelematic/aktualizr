#ifndef BOOTLOADER_CONFIG_H_
#define BOOTLOADER_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <ostream>

enum class RollbackMode { kBootloaderNone = 0, kUbootGeneric, kUbootMasked };
std::ostream& operator<<(std::ostream& os, RollbackMode mode);

struct BootloaderConfig {
  RollbackMode rollback_mode{RollbackMode::kBootloaderNone};
  boost::filesystem::path reboot_sentinel_dir{"/var/run/aktualizr-session"};
  boost::filesystem::path reboot_sentinel_name{"need_reboot"};
  std::string reboot_command{"/sbin/reboot"};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

#endif  // BOOTLOADER_CONFIG_H_
