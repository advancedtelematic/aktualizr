#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_

#include <string>

#include <sys/socket.h>
#include <boost/filesystem.hpp>
#include "utilities/types.h"

namespace Uptane {

enum SecondaryType {
  kVirtual,      // Virtual secondary (in-process fake implementation).
  kLegacy,       // legacy non-UPTANE secondary. All the UPTANE metadata is managed locally. All commands are sent to an
                 // external firmware loader via shell.
  kOpcuaUptane,  // Uptane protocol over OPC-UA
  kIpUptane,     // Custom Uptane protocol over TCP/IP network
  kVirtualUptane,  // Partial UPTANE secondary implemented inside primary
};

class SecondaryConfig {
 public:
  SecondaryType secondary_type;
  std::string ecu_serial;
  std::string ecu_hardware_id;
  bool partial_verifying;
  std::string ecu_private_key;
  std::string ecu_public_key;
  KeyType key_type{kRSA2048};

  std::string opcua_lds_url;

  boost::filesystem::path full_client_dir;   // kVirtual, kLegacy
  boost::filesystem::path firmware_path;     // kVirtual, kLegacy
  boost::filesystem::path metadata_path;     // kVirtual, kLegacy
  boost::filesystem::path target_name_path;  // kVirtual, kLegacy

  boost::filesystem::path flasher;  // kLegacy

  sockaddr_storage ip_addr;  // kIpUptane
};
}  // namespace Uptane

#endif
