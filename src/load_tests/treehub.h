#ifndef LT_TREEHUB_H
#define LT_TREEHUB_H

#include <boost/filesystem.hpp>

void fetchFromOstree(const boost::filesystem::path &baseDir, const boost::filesystem::path &outputDir,
                     const std::string &branchName, const std::string &remoteUrl, const uint rate, const uint nr,
                     const uint parallelism);

#endif  // LT_TREEHUB_H