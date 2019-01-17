#ifndef AKTUALIZR_INFO_CONFIG_H_
#define AKTUALIZR_INFO_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "logging/logging_config.h"
#include "package_manager/packagemanagerconfig.h"
#include "storage/storage_config.h"
#include "utilities/config_utils.h"

// Try to keep the order of config options the same as in
// AktualizrInfoConfig::writeToStream() and
// AktualizrInfoConfig::updateFromPropertyTree().

class AktualizrInfoConfig : public BaseConfig {
 public:
  AktualizrInfoConfig() = default;
  AktualizrInfoConfig(const boost::program_options::variables_map& cmd);
  explicit AktualizrInfoConfig(const boost::filesystem::path& filename);

  void postUpdateValues();
  void writeToStream(std::ostream& sink) const;

  // from primary config
  LoggerConfig logger;
  PackageConfig pacman;
  StorageConfig storage;

 private:
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt) override;

  bool loglevel_from_cmdline{false};
};

#endif  // AKTUALIZR_INFO_CONFIG_H_
