#ifndef LT_CONTEXT_H_
#define LT_CONTEXT_H_

#include <vector>
#include <libaktualizr/config.h>

Config configure(const boost::filesystem::path& cfgFile, const int logLevel);

std::vector<Config> loadDeviceConfigurations(const boost::filesystem::path& baseDir);

#endif
