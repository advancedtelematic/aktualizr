#ifndef PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
#define PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <string>

#include "utilities/config_utils.h"

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

template <>
inline void CopyFromConfig(PackageManager& dest, const std::string& option_name,
                           const boost::property_tree::ptree& pt) {
  boost::optional<std::string> value = pt.get_optional<std::string>(option_name);
  if (value.is_initialized()) {
    std::string pm_type{StripQuotesFromStrings(value.get())};
    if (pm_type == "ostree") {
      dest = kOstree;
    } else if (pm_type == "debian") {
      dest = kDebian;
    } else {
      dest = kNone;
    }
  }
}

#endif  // PACKAGE_MANAGER_PACKAGEMANAGERCONFIG_H_
