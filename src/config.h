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
#include "invstorage.h"
#include "logging.h"
#include "uptane/secondaryconfig.h"
#include "utilities/keymanager.h"
#include "utilities/p11engine.h"
#include "utilities/types.h"

enum ProvisionMode { kAutomatic = 0, kImplicit };
enum PackageManager { kNone = 0, kOstree, kDebian };

// Keep the order of config options the same as in writeToFile() and
// updateFromPropertyTree() in config.cc.

struct GatewayConfig {
  bool socket{false};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct NetworkConfig {
  std::string socket_commands_path{"/tmp/sota-commands.socket"};
  std::string socket_events_path{"/tmp/sota-events.socket"};
  std::vector<std::string> socket_events{"DownloadComplete", "DownloadFailed"};

  std::string ipdiscovery_host{"::ffff:127.0.0.1"};
  in_port_t ipdiscovery_port{9031};
  uint32_t ipdiscovery_wait_seconds{10};
  in_port_t ipuptane_port{9030};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct TlsConfig {
  std::string server;
  boost::filesystem::path server_url_path;
  CryptoSource ca_source{kFile};
  CryptoSource pkey_source{kFile};
  CryptoSource cert_source{kFile};

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
  ProvisionMode mode{kAutomatic};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct UptaneConfig {
  bool polling{true};
  unsigned long long polling_sec{10u};
  std::string device_id;
  std::string primary_ecu_serial;
  std::string primary_ecu_hardware_id;
  std::string director_server;
  std::string repo_server;
  CryptoSource key_source{kFile};
  KeyType key_type{kRSA2048};
  std::vector<Uptane::SecondaryConfig> secondary_configs{};

  std::string getKeyTypeString() const { return keyTypeToString(key_type); }

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct DiscoveryConfig {
  bool ipuptane{false};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct PackageConfig {
  PackageManager type{kOstree};
  std::string os;
  boost::filesystem::path sysroot;
  std::string ostree_server;
  boost::filesystem::path packages_file{"/usr/package.manifest"};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

class Config {
 public:
  Config();
  Config(const boost::filesystem::path& filename, const boost::program_options::variables_map& cmd);
  Config(const boost::filesystem::path& filename);
  Config(const std::vector<boost::filesystem::path>& config_dirs) {
    config_dirs_ = config_dirs;
    updateFromDirs();
  }

  KeyManagerConfig keymanagerConfig() const;

  void updateFromTomlString(const std::string& contents);
  void postUpdateValues();
  void writeToFile(const boost::filesystem::path& filename) const;

  // config data structures
  GatewayConfig gateway;
  NetworkConfig network;
  P11Config p11;
  TlsConfig tls;
  ProvisionConfig provision;
  UptaneConfig uptane;
  DiscoveryConfig discovery;
  PackageConfig pacman;
  StorageConfig storage;
  ImportConfig import;

 private:
  std::vector<boost::filesystem::path> config_dirs_ = {"/usr/lib/sota/conf.d", "/etc/sota/conf.d/"};
  void updateFromDirs();
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void updateFromToml(const boost::filesystem::path& filename);
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void readSecondaryConfigs(const std::vector<boost::filesystem::path>& sconfigs);
  void checkLegacyVersion(const boost::filesystem::path& legacy_interface);
  void initLegacySecondaries(const boost::filesystem::path& legacy_interface);
};

#endif  // CONFIG_H_
