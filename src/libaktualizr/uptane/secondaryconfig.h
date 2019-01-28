#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_

#include <string>

#include <sys/socket.h>
#include <boost/filesystem.hpp>
#include "logging/logging.h"
#include "utilities/exceptions.h"
#include "utilities/types.h"
#include "utilities/utils.h"

namespace Uptane {

enum class SecondaryType {

  kVirtual,  // Virtual secondary (in-process fake implementation).

  kLegacy,  // Deprecated. Do not use.

  kOpcuaUptane,  // Uptane protocol over OPC-UA

  kIpUptane,  // Custom Uptane protocol over TCP/IP network

  kVirtualUptane,  // Partial UPTANE secondary implemented inside primary
  kIsoTpUptane,    // Custom Uptane protocol over ISO/TP network

};

class SecondaryConfig {
 public:
  SecondaryConfig() = default;
  SecondaryConfig(const boost::filesystem::path &config_file);
  SecondaryType secondary_type{};
  std::string ecu_serial;
  std::string ecu_hardware_id;
  bool partial_verifying{};
  std::string ecu_private_key;
  std::string ecu_public_key;
  KeyType key_type{KeyType::kRSA2048};

  std::string opcua_lds_url;

  boost::filesystem::path full_client_dir;   // SecondaryType::kVirtual
  boost::filesystem::path firmware_path;     // SecondaryType::kVirtual
  boost::filesystem::path metadata_path;     // SecondaryType::kVirtual
  boost::filesystem::path target_name_path;  // SecondaryType::kVirtual

  sockaddr_storage ip_addr{};  // SecondaryType::kIpUptane

  uint16_t can_id{0x0000};  // SecondaryType::kIsoTpUptane;
  std::string can_iface;    // SecondaryType::kIsoTpUptane;
};
}  // namespace Uptane

#endif
