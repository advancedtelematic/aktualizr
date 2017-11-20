#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_

#include <string>

#include <boost/filesystem.hpp>

namespace Uptane {

enum SecondaryType {
  kVirtual,  // Virtual secondary (in-process fake implementation).
  kLegacy,   // legacy non-UPTANE secondary. All the UPTANE metadata is managed locally. All commands are sent to an
             // external firmware loader via shell.
  kUptane,   // UPTANE-compliant secondary (UDS, DoIP, et cetera).
};

class SecondaryConfig {
 public:
  std::string ecu_private_key() const;  // kVirtual, kLegacy
  std::string ecu_public_key() const;   // kVirtual, kLegacy

  SecondaryType secondary_type;
  std::string ecu_serial;
  std::string ecu_hardware_id;
  bool partial_verifying;
  std::string ecu_private_key_;
  std::string ecu_public_key_;

  boost::filesystem::path full_client_dir;     // kVirtual, kLegacy
  boost::filesystem::path firmware_path;       // kVirtual
  boost::filesystem::path time_path;           // kVirtual
  boost::filesystem::path previous_time_path;  // kVirtual
  boost::filesystem::path target_name_path;    // kVirtual
};
}

#endif
