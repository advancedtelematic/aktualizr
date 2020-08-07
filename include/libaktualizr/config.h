#ifndef CONFIG_H_
#define CONFIG_H_

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "libaktualizr/types.h"

// Try to keep the order of config options the same as in Config::writeToStream()
// and Config::updateFromPropertyTree() in config.cc.

struct LoggerConfig {
  int loglevel{2};
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

// declare p11 types as incomplete so that the header can be used without libp11
struct PKCS11_ctx_st;
struct PKCS11_slot_st;

struct P11Config {
  boost::filesystem::path module;
  std::string pass;
  std::string uptane_key_id;
  std::string tls_cacert_id;
  std::string tls_pkey_id;
  std::string tls_clientcert_id;

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

struct ProvisionConfig {
  std::string server;
  std::string p12_password;
  std::string expiry_days{"36000"};
  boost::filesystem::path provision_path;
  ProvisionMode mode{ProvisionMode::kDefault};
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

// TODO: move these to their corresponding headers
#define PACKAGE_MANAGER_NONE "none"
#define PACKAGE_MANAGER_OSTREE "ostree"
#define PACKAGE_MANAGER_OSTREEDOCKERAPP "ostree+docker-app"

#ifdef BUILD_OSTREE
#define PACKAGE_MANAGER_DEFAULT PACKAGE_MANAGER_OSTREE
#else
#define PACKAGE_MANAGER_DEFAULT PACKAGE_MANAGER_NONE
#endif

struct PackageConfig {
  std::string type{PACKAGE_MANAGER_DEFAULT};

  // OSTree options
  std::string os;
  boost::filesystem::path sysroot;
  std::string ostree_server;
  boost::filesystem::path images_path{"/var/sota/images"};
  boost::filesystem::path packages_file{"/usr/package.manifest"};

  // Options for simulation (to be used with "none")
  bool fake_need_reboot{false};

  // for specialized configuration
  std::map<std::string, std::string> extra;

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct StorageConfig {
  StorageType type{StorageType::kSqlite};
  boost::filesystem::path path{"/var/sota"};

  // FS storage
  utils::BasedPath uptane_metadata_path{"metadata"};
  utils::BasedPath uptane_private_key_path{"ecukey.der"};
  utils::BasedPath uptane_public_key_path{"ecukey.pub"};
  utils::BasedPath tls_cacert_path{"root.crt"};
  utils::BasedPath tls_pkey_path{"pkey.pem"};
  utils::BasedPath tls_clientcert_path{"client.pem"};

  // SQLite storage
  utils::BasedPath sqldb_path{"sql.db"};  // based on `/var/sota`

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct ImportConfig {
  boost::filesystem::path base_path{"/var/sota/import"};
  utils::BasedPath uptane_private_key_path{""};
  utils::BasedPath uptane_public_key_path{""};
  utils::BasedPath tls_cacert_path{""};
  utils::BasedPath tls_pkey_path{""};
  utils::BasedPath tls_clientcert_path{""};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

/**
 * @brief The TelemetryConfig struct
 * Report device network information: IP address, hostname, MAC address.
 */
struct TelemetryConfig {
  bool report_network{true};
  bool report_config{true};
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

enum class RollbackMode { kBootloaderNone = 0, kUbootGeneric, kUbootMasked };
std::ostream& operator<<(std::ostream& os, RollbackMode mode);

struct BootloaderConfig {
  RollbackMode rollback_mode{RollbackMode::kBootloaderNone};
  boost::filesystem::path reboot_sentinel_dir{"/var/run/aktualizr-session"};
  boost::filesystem::path reboot_sentinel_name{"need_reboot"};
  std::string reboot_command{"/sbin/reboot"};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

// bundle some parts of the main config together
// Should be derived by calling Config::keymanagerConfig()
struct KeyManagerConfig {
  KeyManagerConfig() = delete;  // only allow construction by initializer list
  P11Config p11;
  CryptoSource tls_ca_source;
  CryptoSource tls_pkey_source;
  CryptoSource tls_cert_source;
  KeyType uptane_key_type;
  CryptoSource uptane_key_source;
};

/**
 * @brief The BaseConfig class
 */
class BaseConfig {
 public:
  virtual ~BaseConfig() = default;
  void updateFromToml(const boost::filesystem::path& filename);
  virtual void updateFromPropertyTree(const boost::property_tree::ptree& pt) = 0;

 protected:
  void updateFromDirs(const std::vector<boost::filesystem::path>& configs);

  static void checkDirs(const std::vector<boost::filesystem::path>& configs) {
    for (const auto& config : configs) {
      if (!boost::filesystem::exists(config)) {
        throw std::runtime_error("Config directory " + config.string() + " does not exist.");
      }
    }
  }

  std::vector<boost::filesystem::path> config_dirs_ = {"/usr/lib/sota/conf.d", "/etc/sota/conf.d/"};
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
  bool loglevel_from_cmdline{false};
};

std::ostream& operator<<(std::ostream& os, const Config& cfg);

#endif  // CONFIG_H_
