#ifndef LT_CONTEXT_H_
#define LT_CONTEXT_H_

#include <config/config.h>
#include <vector>

Config configure(const boost::filesystem::path& cfgFile, const int logLevel);

std::vector<Config> loadDeviceConfigurations(const boost::filesystem::path& baseDir);

#endif