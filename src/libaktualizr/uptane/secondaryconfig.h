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
};

class SecondaryConfig {
 public:
  SecondaryConfig() = default;
  SecondaryConfig(const boost::filesystem::path &config_file) {
    if (!boost::filesystem::exists(config_file)) {
      throw FatalException(config_file.string() + " does not exist!");
    }
    Json::Value config_json = Utils::parseJSONFile(config_file);

    std::string stype = config_json["secondary_type"].asString();
    if (stype == "virtual") {
      secondary_type = Uptane::SecondaryType::kVirtual;
    } else if (stype == "legacy") {
      throw FatalException("Legacy secondaries are deprecated.");
    } else if (stype == "ip_uptane") {
      secondary_type = Uptane::SecondaryType::kIpUptane;
    } else if (stype == "opcua_uptane") {
      secondary_type = Uptane::SecondaryType::kOpcuaUptane;
    } else {
      LOG_ERROR << "Unrecognized secondary type: " << stype;
    }
    ecu_serial = config_json["ecu_serial"].asString();
    ecu_hardware_id = config_json["ecu_hardware_id"].asString();
    partial_verifying = config_json["partial_verifying"].asBool();
    ecu_private_key = config_json["ecu_private_key"].asString();
    ecu_public_key = config_json["ecu_public_key"].asString();

    full_client_dir = boost::filesystem::path(config_json["full_client_dir"].asString());
    firmware_path = boost::filesystem::path(config_json["firmware_path"].asString());
    metadata_path = boost::filesystem::path(config_json["metadata_path"].asString());
    target_name_path = boost::filesystem::path(config_json["target_name_path"].asString());

    std::string key_type_str = config_json["key_type"].asString();
    if (key_type_str.size() != 0u) {
      if (key_type_str == "RSA2048") {
        key_type = KeyType::kRSA2048;
      } else if (key_type_str == "RSA3072") {
        key_type = KeyType::kRSA3072;
      } else if (key_type_str == "RSA4096") {
        key_type = KeyType::kRSA4096;
      } else if (key_type_str == "ED25519") {
        key_type = KeyType::kED25519;
      }
    }
  }
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
};
}  // namespace Uptane

#endif
