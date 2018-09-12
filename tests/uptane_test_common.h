#ifndef UPTANE_TEST_COMMON_H_
#define UPTANE_TEST_COMMON_H_

#include <string>

#include "config/config.h"
#include "uptane/secondaryconfig.h"
#include "utilities/utils.h"

struct UptaneTestCommon {
  static Uptane::SecondaryConfig addDefaultSecondary(Config& config, const TemporaryDirectory& temp_dir,
                                                     const std::string& serial, const std::string& hw_id) {
    Uptane::SecondaryConfig ecu_config;
    ecu_config.secondary_type = Uptane::SecondaryType::kVirtual;
    ecu_config.partial_verifying = false;
    ecu_config.full_client_dir = temp_dir.Path();
    ecu_config.ecu_serial = serial;
    ecu_config.ecu_hardware_id = hw_id;
    ecu_config.ecu_private_key = "sec.priv";
    ecu_config.ecu_public_key = "sec.pub";
    ecu_config.firmware_path = temp_dir / "firmware.txt";
    ecu_config.target_name_path = temp_dir / "firmware_name.txt";
    ecu_config.metadata_path = temp_dir / "secondary_metadata";
    config.uptane.secondary_configs.push_back(ecu_config);
    return ecu_config;
  }
};

#endif // UPTANE_TEST_COMMON_H_


