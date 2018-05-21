#include "context.h"
#include <boost/program_options.hpp>
#include "logging/logging.h"

using namespace boost::filesystem;
namespace po = boost::program_options;

Config configure(const path& cfgFile, const int logLevel) {
  po::variables_map vm;
  vm.insert(std::make_pair("loglevel", po::variable_value(logLevel, false)));
  const std::vector<path> configDirs{cfgFile};
  vm.insert(std::make_pair("config", po::variable_value(configDirs, false)));
  po::notify(vm);
  return Config{vm};
}

std::vector<Config> loadDeviceConfigurations(const path& baseDir) {
  const int severity = loggerGetSeverity();
  std::vector<Config> configs;
  for (directory_entry& x : directory_iterator(baseDir)) {
    const path sotaToml = x / "sota.toml";
    configs.push_back(configure(sotaToml, severity));
  }
  return configs;
}