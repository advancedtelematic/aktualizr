#include "package_manager/packagemanagerconfig.h"

#include <boost/log/trivial.hpp>

#include "utilities/config_utils.h"

void PackageConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  std::string pm_type = "ostree";
  CopyFromConfig(pm_type, "type", boost::log::trivial::trace, pt);
  if (pm_type == "ostree") {
    type = kOstree;
  } else if (pm_type == "debian") {
    type = kDebian;
  } else {
    type = kNone;
  }

  CopyFromConfig(os, "os", boost::log::trivial::trace, pt);
  CopyFromConfig(sysroot, "sysroot", boost::log::trivial::trace, pt);
  CopyFromConfig(ostree_server, "ostree_server", boost::log::trivial::trace, pt);
  CopyFromConfig(packages_file, "packages_file", boost::log::trivial::trace, pt);
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