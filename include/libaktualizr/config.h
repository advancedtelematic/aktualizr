#ifndef CONFIG_H_
#define CONFIG_H_

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <libaktualizr/bootloader_config.h>
#include <libaktualizr/keymanager_config.h>
#include <libaktualizr/p11_config.h>
#include <libaktualizr/logging_config.h>
#include <libaktualizr/packagemanagerconfig.h>
#include <libaktualizr/storage_config.h>
#include <libaktualizr/telemetryconfig.h>
#include <libaktualizr/config_utils.h>
#include <libaktualizr/types.h>

enum class ProvisionMode { kSharedCred = 0, kDeviceCred };

// Try to keep the order of config options the same as in Config::writeToStream()
// and Config::updateFromPropertyTree() in config.cc.

struct TlsConfig {
  std::string server;
  boost::filesystem::path server_url_path;
  CryptoSource ca_source{CryptoSource::kFile};
  CryptoSource pkey_source{CryptoSource::kFile};
  CryptoSource cert_source{CryptoSource::kFile};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct ProvisionConfig {
  std::string server;
  std::string p12_password;
  std::string expiry_days{"36000"};
  boost::filesystem::path provision_path;
  ProvisionMode mode{ProvisionMode::kSharedCred};
  std::string device_id;
  std::string primary_ecu_serial;
  std::string primary_ecu_hardware_id;
  std::string ecu_registration_endpoint;

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct UptaneConfig {
  uint64_t polling_sec{10U};
  std::string director_server;
  std::string repo_server;
  CryptoSource key_source{CryptoSource::kFile};
  KeyType key_type{KeyType::kRSA2048};
  bool force_install_completion{false};
  boost::filesystem::path secondary_config_file;
  uint64_t secondary_preinstall_wait_sec{600U};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

/**
 * Configuration object for an aktualizr instance running on a Primary ECU.
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
  P11Config p11;
  TlsConfig tls;
  ProvisionConfig provision;
  UptaneConfig uptane;
  PackageConfig pacman;
  StorageConfig storage;
  ImportConfig import;
  TelemetryConfig telemetry;
  BootloaderConfig bootloader;

 private:
  void updateFromPropertyTree(const boost::property_tree::ptree& pt) override;
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);

  std::vector<boost::filesystem::path> config_dirs_ = {"/usr/lib/sota/conf.d", "/etc/sota/conf.d/"};
  bool loglevel_from_cmdline{false};
};

std::ostream& operator<<(std::ostream& os, const Config& cfg);

#endif  // CONFIG_H_
