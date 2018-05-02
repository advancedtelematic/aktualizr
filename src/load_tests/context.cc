#include "context.h"

using namespace boost::filesystem;

std::vector<Config> loadDeviceConfigurations(const path& baseDir) {
  std::vector<Config> configs;
  for(directory_entry &x: directory_iterator(baseDir)) {
    const path sotaToml = x / "sota.toml";
    configs.push_back(Config {sotaToml});
  }
  return configs;
}