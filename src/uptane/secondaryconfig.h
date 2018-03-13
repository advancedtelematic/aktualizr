#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_

#include <string>

#include <boost/filesystem.hpp>
#include "types.h"

namespace Uptane {

enum SecondaryType {
  kVirtual,  // Virtual secondary (in-process fake implementation).
  kLegacy,   // legacy non-UPTANE secondary. All the UPTANE metadata is managed locally. All commands are sent to an
             // external firmware loader via shell.
  kUptane,   // UPTANE-compliant secondary (UDS, DoIP, et cetera).
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

  boost::filesystem::path full_client_dir;   // kVirtual, kLegacy
  boost::filesystem::path firmware_path;     // kVirtual, kLegacy
  boost::filesystem::path metadata_path;     // kVirtual, kLegacy
  boost::filesystem::path target_name_path;  // kVirtual, kLegacy

  boost::filesystem::path flasher;  // kLegacy
};
}  // namespace Uptane

#endif
