#include "package_manager/packagemanagerconfig.h"

#include <boost/log/trivial.hpp>

void PackageConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(type, "type", pt);
  CopyFromConfig(os, "os", pt);
  CopyFromConfig(sysroot, "sysroot", pt);
  CopyFromConfig(ostree_server, "ostree_server", pt);
  CopyFromConfig(packages_file, "packages_file", pt);
}

void PackageConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, type, "type");
  writeOption(out_stream, os, "os");
  writeOption(out_stream, sysroot, "sysroot");
  writeOption(out_stream, ostree_server, "ostree_server");
  writeOption(out_stream, packages_file, "packages_file");
}

std::ostream& operator<<(std::ostream& os, PackageManager pm) {
  std::string pm_str;
  switch (pm) {
    case kOstree:
      pm_str = "ostree";
      break;
    case kDebian:
      pm_str = "debian";
      break;
    case kNone:
    default:
      pm_str = "none";
      break;
  }
  os << '"' << pm_str << '"';
  return os;
}
