#ifndef PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
#define PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <string>

#include "utilities/config_utils.h"

enum class PackageManager { kNone = 0, kOstree, kDebian, kAndroid };

struct PackageConfig {
  PackageManager type{PackageManager::kOstree};
  std::string os;
  boost::filesystem::path sysroot;
  std::string ostree_server;
  boost::filesystem::path packages_file{"/usr/package.manifest"};

  // Options for simulation (to be used with kNone)
  bool fake_need_reboot{false};

  void updateFromPropertyTree(const boost::property_tree::ptree& pt);
  void writeToStream(std::ostream& out_stream) const;
};

template <>
inline void CopyFromConfig(PackageManager& dest, const std::string& option_name,
                           const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    std::string pm_type{StripQuotesFromStrings(value.get())};
    if (pm_type == "ostree") {
      dest = PackageManager::kOstree;
    } else if (pm_type == "debian") {
      dest = PackageManager::kDebian;
    } else if (pm_type == "android") {
      dest = PackageManager::kAndroid;
    } else {
      dest = PackageManager::kNone;
    }
  }
}

#endif  // PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
