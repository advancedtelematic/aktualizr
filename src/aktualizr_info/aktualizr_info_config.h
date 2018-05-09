#ifndef AKTUALIZR_INFO_CONFIG_H_
#define AKTUALIZR_INFO_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "config/config.h"
#include "logging/logging.h"

// Try to keep the order of config options the same as in
// AktualizrInfoConfig::writeToFile() and
// AktualizrInfoConfig::updateFromPropertyTree().

class AktualizrInfoConfig {
 public:
  AktualizrInfoConfig() = default;
  AktualizrInfoConfig(const boost::program_options::variables_map& cmd);
  explicit AktualizrInfoConfig(const boost::filesystem::path& filename);

  void postUpdateValues();
  void writeToFile(const boost::filesystem::path& filename);

  // from primary config
  LoggerConfig logger;
  StorageConfig storage;

 private:
  void updateFromDirs(const std::vector<boost::filesystem::path>& configs);
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void updateFromToml(const boost::filesystem::path& filename);

  std::vector<boost::filesystem::path> config_dirs_ = {"/usr/lib/sota/conf.d", "/etc/sota/conf.d/"};
  bool loglevel_from_cmdline{false};
};

#endif  // AKTUALIZR_INFO_CONFIG_H_
