#ifndef AKTUALIZR_SECONDARY_CONFIG_H_
#define AKTUALIZR_SECONDARY_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "bootloader/bootloader_config.h"
#include "crypto/keymanager_config.h"
#include "crypto/p11_config.h"
#include "logging/logging_config.h"
#include "package_manager/packagemanagerconfig.h"
#include "storage/storage_config.h"
#include "utilities/config_utils.h"

// Try to keep the order of config options the same as in
// AktualizrSecondaryConfig::writeToStream() and
// AktualizrSecondaryConfig::updateFromPropertyTree().

struct AktualizrSecondaryNetConfig {
  in_port_t port{9030};
  std::string primary_ip;
  in_port_t primary_port{9030};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

struct AktualizrSecondaryUptaneConfig {
  std::string ecu_serial;
  std::string ecu_hardware_id;
  CryptoSource key_source{CryptoSource::kFile};
  KeyType key_type{KeyType::kRSA2048};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

class AktualizrSecondaryConfig : public BaseConfig {
 public:
  AktualizrSecondaryConfig() = default;
  AktualizrSecondaryConfig(const boost::program_options::variables_map& cmd);
  explicit AktualizrSecondaryConfig(const boost::filesystem::path& filename);

  KeyManagerConfig keymanagerConfig() const;

  void postUpdateValues();
  void writeToStream(std::ostream& sink) const;

  // from primary config
  LoggerConfig logger;

  AktualizrSecondaryNetConfig network;
  AktualizrSecondaryUptaneConfig uptane;

  // from primary config
  P11Config p11;
  PackageConfig pacman;
  BootloaderConfig bootloader;
  StorageConfig storage;
  ImportConfig import;

 private:
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt) override;
};
std::ostream& operator<<(std::ostream& os, const AktualizrSecondaryConfig& cfg);

#endif  // AKTUALIZR_SECONDARY_CONFIG_H_
