#ifndef PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
#define PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
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
  std::string os;
  boost::filesystem::path sysroot;
  std::string ostree_server;
  boost::filesystem::path packages_file{"/usr/package.manifest"};

#ifdef BUILD_DOCKERAPP
  std::vector<std::string> docker_apps;
  boost::filesystem::path docker_apps_root;
  boost::filesystem::path docker_app_params;
  boost::filesystem::path docker_app_bin{"/usr/bin/docker-app"};
  boost::filesystem::path docker_compose_bin{"/usr/bin/docker-compose"};
#endif

  // Options for simulation ()
  bool fake_need_reboot{false};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

#endif  // PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
