#ifndef AKTUALIZR_INFO_CONFIG_H_
#define AKTUALIZR_INFO_CONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "libaktualizr/config.h"

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

  // from Primary config
  BootloaderConfig bootloader;
  LoggerConfig logger;
  PackageConfig pacman;
  StorageConfig storage;

 private:
  void updateFromCommandLine(const boost::program_options::variables_map& cmd);
  void updateFromPropertyTree(const boost::property_tree::ptree& pt) override;

  bool loglevel_from_cmdline{false};
};
std::ostream& operator<<(std::ostream& os, const AktualizrInfoConfig& cfg);

#endif  // AKTUALIZR_INFO_CONFIG_H_
