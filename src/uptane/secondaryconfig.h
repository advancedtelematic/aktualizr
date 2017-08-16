#ifndef UPTANE_SECONDARYCONFIG_H_
#define UPTANE_SECONDARYCONFIG_H_

#include <string>

#include <boost/filesystem.hpp>

namespace Uptane {

enum SecondaryBusType {
  kVirtualBus,    // Virtual secondary bus, implemented in TestBusSecondary class
  kNonUptaneBus,  // Bus for legacy secondaries. All the UPTANE metadata is managed locally, firmware blob is shelled
                  // out to an external firmware loader. Not implemented yet.
  kAtsUds,        // UPTANE secondaries implementing ATS's flavour of UDS. Not implemented yet.
};

class SecondaryConfig {
 public:
  std::string ecu_private_key() const;  // kVirtualBus, kNonUptaneBus
  std::string ecu_public_key() const;   // kVirtualBus, kNonUptaneBus

  SecondaryBusType bus_type;
  std::string ecu_serial;
  std::string ecu_hardware_id;
  bool partial_verifying;
  std::string ecu_private_key_;
  std::string ecu_public_key_;

  boost::filesystem::path full_client_dir;     // kVirtualBus, kNonUptaneBus
  boost::filesystem::path firmware_path;       // kVirtualBus
  boost::filesystem::path time_path;           // kVirtualBus
  boost::filesystem::path previous_time_path;  // kVirtualBus
  boost::filesystem::path target_name_path;    // kVirtualBus
};
}

#endif
