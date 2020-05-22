#ifndef PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
#define PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <map>
#include <string>

#include "utilities/config_utils.h"

// TODO: move these to their corresponding headers
#define PACKAGE_MANAGER_NONE "none"
#define PACKAGE_MANAGER_OSTREE "ostree"
#define PACKAGE_MANAGER_DEBIAN "debian"
#define PACKAGE_MANAGER_ANDROID "android"
#define PACKAGE_MANAGER_OSTREEDOCKERAPP "ostree+docker-app"

#ifdef BUILD_OSTREE
#define PACKAGE_MANAGER_DEFAULT PACKAGE_MANAGER_OSTREE
#else
#define PACKAGE_MANAGER_DEFAULT PACKAGE_MANAGER_NONE
#endif

struct PackageConfig {
  std::string type{PACKAGE_MANAGER_DEFAULT};

  // OSTree options
  std::string os;
  boost::filesystem::path sysroot;
  std::string ostree_server;
  boost::filesystem::path packages_file{"/usr/package.manifest"};

  // Options for simulation (to be used with "none")
  bool fake_need_reboot{false};

  // for specialized configuration
  std::map<std::string, std::string> extra;

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

#endif  // PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
