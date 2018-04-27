#ifndef PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
#define PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <string>

enum PackageManager { kNone = 0, kOstree, kDebian };

struct PackageConfig {
  PackageManager type{kOstree};
  std::string os;
  boost::filesystem::path sysroot;
  std::string ostree_server;
  boost::filesystem::path packages_file{"/usr/package.manifest"};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

#endif  // PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_