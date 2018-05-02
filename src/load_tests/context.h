#ifndef LT_CONTEXT_H_
#define LT_CONTEXT_H_

#include <vector>
#include <config/config.h>

std::vector<Config> loadDeviceConfigurations(const boost::filesystem::path& baseDir);

#endif