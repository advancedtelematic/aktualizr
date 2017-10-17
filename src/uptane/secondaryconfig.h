#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_

#include <string>

#include <boost/filesystem.hpp>

namespace Uptane {
class SecondaryConfig {
 public:
  std::string ecu_private_key() const;
  std::string ecu_public_key() const;

  std::string ecu_serial;
  std::string ecu_hardware_id;
  std::string ecu_private_key_;
  std::string ecu_public_key_;
  boost::filesystem::path full_client_dir;
  bool partial_verifying;
  boost::filesystem::path firmware_path;
};
}

#endif
