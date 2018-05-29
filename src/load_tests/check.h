#ifndef LT_CHECK_H_
#define LT_CHECK_H_

#include <boost/filesystem.hpp>
#include <string>

void checkForUpdates(const boost::filesystem::path &baseDir, const unsigned int rate, const unsigned int nr,
                     const unsigned int parallelism);

#endif
