#ifndef AKTUALIZR_SECONDARY_CONFIG_H_
#define AKTUALIZR_SECONDARY_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "config.h"

struct AktualizrSecondaryNetConfig {
  in_port_t port{9030};
  bool discovery{true};
  in_port_t discovery_port{9031};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct AktualizrSecondaryUptaneConfig {
  std::string ecu_serial;
  std::string ecu_hardware_id;
  CryptoSource key_source{kFile};
  KeyType key_type{kRSA2048};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

class AktualizrSecondaryConfig {
 public:
  AktualizrSecondaryConfig() = default;
  AktualizrSecondaryConfig(const boost::filesystem::path& filename, const boost::program_options::variables_map& cmd);
  explicit AktualizrSecondaryConfig(const boost::filesystem::path& filename);

  KeyManagerConfig keymanagerConfig() const;

  void writeToFile(const boost::filesystem::path& filename);

  AktualizrSecondaryNetConfig network;
  AktualizrSecondaryUptaneConfig uptane;

  // from primary config
  StorageConfig storage;
  P11Config p11;
  PackageConfig pacman;

 private:
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void updateFromToml(const boost::filesystem::path& filename);
};

#endif  // AKTUALIZR_SECONDARY_CONFIG_H_
