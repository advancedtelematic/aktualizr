#ifndef LT_CHECK_H_
#define LT_CHECK_H_

#include <string>
#include <boost/filesystem.hpp>

void checkForUpdates(const boost::filesystem::path &baseDir, uint rate, const uint nr, const uint parallelism);

#endif
