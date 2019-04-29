#ifndef UPTANE_TEST_COMMON_H_
#define UPTANE_TEST_COMMON_H_

#include <string>
#include <vector>

#include "json/json.h"

#include "config/config.h"
#include "uptane/secondaryconfig.h"
#include "uptane/tuf.h"
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

  static Config makeTestConfig(const TemporaryDirectory& temp_dir, const std::string& url) {
    Config conf("tests/config/basic.toml");
    conf.uptane.director_server = url + "/director";
    conf.uptane.repo_server = url + "/repo";
    conf.provision.server = url;
    conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
    conf.provision.primary_ecu_hardware_id = "primary_hw";
    conf.storage.path = temp_dir.Path();
    conf.tls.server = url;
    conf.bootloader.reboot_sentinel_dir = temp_dir.Path();
    UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");
    return conf;
  }

  static std::vector<Uptane::Target> makePackage(const std::string &serial, const std::string &hw_id) {
    std::vector<Uptane::Target> packages_to_install;
    Json::Value ot_json;
    ot_json["custom"]["ecuIdentifiers"][serial]["hardwareId"] = hw_id;
    ot_json["custom"]["targetFormat"] = "OSTREE";
    ot_json["length"] = 0;
    ot_json["hashes"]["sha256"] = serial;
    packages_to_install.emplace_back(serial, ot_json);
    return packages_to_install;
  }
};

#endif // UPTANE_TEST_COMMON_H_


