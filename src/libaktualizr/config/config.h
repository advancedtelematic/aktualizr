#ifndef CONFIG_H_
#define CONFIG_H_

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/uuid/uuid.hpp>             // uuid class
#include <boost/uuid/uuid_generators.hpp>  // generators
#include <boost/uuid/uuid_io.hpp>

#include "asn1/asn1-cerstream.h"
#include "bootloader/bootloader.h"
#include "crypto/keymanager_config.h"
#include "crypto/p11_config.h"
#include "logging/logging_config.h"
#include "package_manager/packagemanagerconfig.h"
#include "storage/storage_config.h"
#include "telemetry/telemetryconfig.h"
#include "uptane/secondaryconfig.h"
#include "utilities/config_utils.h"
#include "utilities/types.h"

enum class ProvisionMode { kAutomatic = 0, kImplicit };

// Try to keep the order of config options the same as in Config::writeToStream()
// and Config::updateFromPropertyTree() in config.cc.

struct NetworkConfig {
  std::string ipdiscovery_host{"127.0.0.1"};
  in_port_t ipdiscovery_port{9031};
  uint32_t ipdiscovery_wait_seconds{2};
  in_port_t ipuptane_port{9030};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct TlsConfig {
  std::string server;
  boost::filesystem::path server_url_path;
  CryptoSource ca_source{CryptoSource::kFile};
  CryptoSource pkey_source{CryptoSource::kFile};
  CryptoSource cert_source{CryptoSource::kFile};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

asn1::Serializer& operator<<(asn1::Serializer& ser, const TlsConfig& tls_conf);
asn1::Deserializer& operator>>(asn1::Deserializer& des, TlsConfig& tls_conf);

struct ProvisionConfig {
  std::string server;
  std::string p12_password;
  std::string expiry_days{"36000"};
  boost::filesystem::path provision_path;
  ProvisionMode mode{ProvisionMode::kAutomatic};
  std::string device_id;
  std::string primary_ecu_serial;
  std::string primary_ecu_hardware_id;
  std::string ecu_registration_endpoint;

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct UptaneConfig {
  RunningMode running_mode{RunningMode::kFull};
  uint64_t polling_sec{10u};
  std::string director_server;
  std::string repo_server;
  CryptoSource key_source{CryptoSource::kFile};
  KeyType key_type{KeyType::kRSA2048};
  boost::filesystem::path legacy_interface{};
  std::vector<Uptane::SecondaryConfig> secondary_configs{};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct DiscoveryConfig {
  bool ipuptane{false};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

/**
 * Configuration object for an aktualizr instance running on a primary ECU.
 *
 * This class is a parent to a series of smaller configuration objects for
 * specific subsystems. Note that most other aktualizr-related tools have their
 * own parent configuration objects with a reduced set of members.
 */
class Config : public BaseConfig {
 public:
  Config();
  explicit Config(const boost::program_options::variables_map& cmd);
  explicit Config(const boost::filesystem::path& filename);
  explicit Config(const std::vector<boost::filesystem::path>& config_dirs);

  KeyManagerConfig keymanagerConfig() const;

  void updateFromTomlString(const std::string& contents);
  void postUpdateValues();
  void writeToStream(std::ostream& sink) const;

  // Config data structures. Keep logger first so that it is taken into account
  // while processing the others.
  LoggerConfig logger;
  NetworkConfig network;
  P11Config p11;
  TlsConfig tls;
  ProvisionConfig provision;
  UptaneConfig uptane;
  DiscoveryConfig discovery;
  PackageConfig pacman;
  StorageConfig storage;
  ImportConfig import;
  TelemetryConfig telemetry;
  BootloaderConfig bootloader;

 private:
  void updateFromPropertyTree(const boost::property_tree::ptree& pt) override;
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void readSecondaryConfigs(const std::vector<boost::filesystem::path>& sconfigs);
  void checkLegacyVersion();
  void initLegacySecondaries();

  std::vector<boost::filesystem::path> config_dirs_ = {"/usr/lib/sota/conf.d", "/etc/sota/conf.d/"};
  bool loglevel_from_cmdline{false};
};

#endif  // CONFIG_H_
