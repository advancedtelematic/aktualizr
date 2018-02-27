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

#include "logging.h"
#include "types.h"
#include "uptane/secondaryconfig.h"

enum ProvisionMode { kAutomatic = 0, kImplicit };
enum CryptoSource { kFile = 0, kPkcs11 };
enum StorageType { kFileSystem = 0, kSqlite };
enum PackageManager { kNone = 0, kOstree, kDebian };

std::ostream& operator<<(std::ostream& os, CryptoSource cs);
// Keep the order of config options the same as in writeToFile() and
// updateFromPropertyTree() in config.cc.

struct GatewayConfig {
  GatewayConfig() : http(true), socket(false) {}
  bool http;
  bool socket;
};

struct NetworkConfig {
  NetworkConfig() : socket_commands_path("/tmp/sota-commands.socket"), socket_events_path("/tmp/sota-events.socket") {
    socket_events.push_back("DownloadComplete");
    socket_events.push_back("DownloadFailed");
  }
  std::string socket_commands_path;
  std::string socket_events_path;
  std::vector<std::string> socket_events;
};

struct P11Config {
  P11Config() {}
  boost::filesystem::path module;
  std::string pass;
  std::string uptane_key_id;
  std::string tls_cacert_id;
  std::string tls_pkey_id;
  std::string tls_clientcert_id;
};

class TlsConfig {
 public:
  TlsConfig() : server(""), server_url_path(""), ca_source(kFile), pkey_source(kFile), cert_source(kFile) {}

  std::string server;
  boost::filesystem::path server_url_path;
  CryptoSource ca_source;
  CryptoSource pkey_source;
  CryptoSource cert_source;

  std::string cer_serialize();
  void cer_deserialize(const std::string& cer);
  TlsConfig(const std::string& cer) { cer_deserialize(cer); }
};

struct ProvisionConfig {
  ProvisionConfig() : server(""), p12_password(""), expiry_days("36000"), provision_path(""), mode(kAutomatic) {}
  std::string server;
  std::string p12_password;
  std::string expiry_days;
  boost::filesystem::path provision_path;
  ProvisionMode mode;
};

struct UptaneConfig {
  UptaneConfig()
      : polling(true),
        polling_sec(10u),
        device_id(""),
        primary_ecu_serial(""),
        primary_ecu_hardware_id(""),
        director_server(""),
        repo_server(""),
        key_source(kFile),
        key_type(kRSA2048) {}
  bool polling;
  unsigned long long polling_sec;
  std::string device_id;
  std::string primary_ecu_serial;
  std::string primary_ecu_hardware_id;
  std::string director_server;
  std::string repo_server;
  CryptoSource key_source;
  KeyType key_type;
  std::string getKeyTypeString() const { return (key_type == kED25519) ? "ED25519" : "RSA"; }
  std::vector<Uptane::SecondaryConfig> secondary_configs;
};

struct PackageConfig {
  PackageConfig() : type(kOstree), os(""), sysroot(""), ostree_server(""), packages_file("/usr/package.manifest") {}
  PackageManager type;
  std::string os;
  boost::filesystem::path sysroot;
  std::string ostree_server;
  boost::filesystem::path packages_file;
};

struct StorageConfig {
  StorageConfig()
      : type(kFileSystem),
        path("/var/sota"),
        sqldb_path("/var/sota/storage.db"),
        uptane_metadata_path("metadata"),
        uptane_private_key_path("ecukey.pem"),
        uptane_public_key_path("ecukey.pub"),
        tls_cacert_path("ca.pem"),
        tls_pkey_path("pkey.pem"),
        tls_clientcert_path("client.pem"),
        schemas_path("/usr/lib/sota/schemas") {}
  StorageType type;
  boost::filesystem::path path;
  boost::filesystem::path sqldb_path;  // TODO: merge with path once SQLStorage class is complete
  // FS storage
  boost::filesystem::path uptane_metadata_path;
  boost::filesystem::path uptane_private_key_path;
  boost::filesystem::path uptane_public_key_path;
  boost::filesystem::path tls_cacert_path;
  boost::filesystem::path tls_pkey_path;
  boost::filesystem::path tls_clientcert_path;

  // SQLite storage
  boost::filesystem::path schemas_path;
};

struct ImportConfig {
  ImportConfig()
      : uptane_private_key_path(""),
        uptane_public_key_path(""),
        tls_cacert_path(""),
        tls_pkey_path(""),
        tls_clientcert_path("") {}
  boost::filesystem::path uptane_private_key_path;
  boost::filesystem::path uptane_public_key_path;
  boost::filesystem::path tls_cacert_path;
  boost::filesystem::path tls_pkey_path;
  boost::filesystem::path tls_clientcert_path;
};

class Config {
 public:
  Config();
  Config(const boost::property_tree::ptree& pt);
  Config(const boost::filesystem::path& filename, const boost::program_options::variables_map& cmd);
  Config(const boost::filesystem::path& filename);

  void updateFromTomlString(const std::string& contents);
  void postUpdateValues();
  void writeToFile(const boost::filesystem::path& filename);

  // config data structures
  GatewayConfig gateway;
  NetworkConfig network;
  P11Config p11;
  TlsConfig tls;
  ProvisionConfig provision;
  UptaneConfig uptane;
  PackageConfig pacman;
  StorageConfig storage;
  ImportConfig import;

 private:
  static std::string stripQuotes(const std::string& value);
  static std::string addQuotes(const std::string& value);
  template <typename T>
  static T StripQuotesFromStrings(const T& value);
  template <typename T>
  static void CopyFromConfig(T& dest, const std::string& option_name, boost::log::trivial::severity_level warning_level,
                             const boost::property_tree::ptree& pt);
  template <typename T>
  static T addQuotesToStrings(const T& value);
  template <typename T>
  static void writeOption(std::ofstream& sink, const T& data, const std::string& option_name);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void updateFromToml(const boost::filesystem::path& filename);
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void readSecondaryConfigs(const std::vector<boost::filesystem::path>& sconfigs);
  void checkLegacyVersion(const boost::filesystem::path& legacy_interface);
  void initLegacySecondaries(const boost::filesystem::path& legacy_interface);
};

#endif  // CONFIG_H_
