#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_

#include <string>

#include <sys/socket.h>
#include <boost/filesystem.hpp>
#include "utilities/types.h"

namespace Uptane {

enum class SecondaryType {
  Virtual,      // Virtual secondary (in-process fake implementation).
  Legacy,       // legacy non-UPTANE secondary. All the UPTANE metadata is managed locally. All commands are sent to an
                // external firmware loader via shell.
  OpcuaUptane,  // Uptane protocol over OPC-UA
  IpUptane,     // Custom Uptane protocol over TCP/IP network
  VirtualUptane,  // Partial UPTANE secondary implemented inside primary
};

class SecondaryConfig {
 public:
  SecondaryType secondary_type{};
  std::string ecu_serial;
  std::string ecu_hardware_id;
  bool partial_verifying{};
  std::string ecu_private_key;
  std::string ecu_public_key;
  KeyType key_type{KeyType::RSA2048};

  std::string opcua_lds_url;

  boost::filesystem::path full_client_dir;   // SecondaryType::Virtual, SecondaryType::Legacy
  boost::filesystem::path firmware_path;     // SecondaryType::Virtual, SecondaryType::Legacy
  boost::filesystem::path metadata_path;     // SecondaryType::Virtual, SecondaryType::Legacy
  boost::filesystem::path target_name_path;  // SecondaryType::Virtual, SecondaryType::Legacy

  boost::filesystem::path flasher;  // SecondaryType::Legacy

  sockaddr_storage ip_addr{};  // SecondaryType::IpUptane
};
}  // namespace Uptane

#endif
