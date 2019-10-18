#include "package_manager/packagemanagerconfig.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/log/trivial.hpp>

std::ostream& operator<<(std::ostream& os, PackageManager pm) {
  std::string pm_str;
  switch (pm) {
    case PackageManager::kOstree:
      pm_str = "ostree";
      break;
    case PackageManager::kDebian:
      pm_str = "debian";
      break;
    case PackageManager::kOstreeDockerApp:
      pm_str = "ostree+docker-app";
      break;
    case PackageManager::kNone:
    default:
      pm_str = "none";
      break;
  }
  os << '"' << pm_str << '"';
  return os;
}

void PackageConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  CopyFromConfig(type, "type", pt);
  CopyFromConfig(os, "os", pt);
  CopyFromConfig(sysroot, "sysroot", pt);
  CopyFromConfig(ostree_server, "ostree_server", pt);
  CopyFromConfig(packages_file, "packages_file", pt);
  CopyFromConfig(fake_need_reboot, "fake_need_reboot", pt);
#ifdef BUILD_DOCKERAPP
  CopyFromConfig(docker_apps_root, "docker_apps_root", pt);
  CopyFromConfig(docker_compose_bin, "docker_compose_bin", pt);
  std::string val;
  CopyFromConfig(val, "docker_apps", pt);
  if (val.length() > 0) {
    // token_compress_on allows lists like: "foo,bar", "foo, bar", or "foo bar"
    boost::split(docker_apps, val, boost::is_any_of(", "), boost::token_compress_on);
    CopyFromConfig(docker_app_params, "docker_app_params", pt);
    CopyFromConfig(docker_app_bin, "docker_app_bin", pt);
  }
#endif
}

void PackageConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, type, "type");
  writeOption(out_stream, os, "os");
  writeOption(out_stream, sysroot, "sysroot");
  writeOption(out_stream, ostree_server, "ostree_server");
  writeOption(out_stream, packages_file, "packages_file");
  writeOption(out_stream, fake_need_reboot, "fake_need_reboot");
#ifdef BUILD_DOCKERAPP
  writeOption(out_stream, boost::algorithm::join(docker_apps, ","), "docker_apps");
  writeOption(out_stream, docker_apps_root, "docker_apps_root");
  writeOption(out_stream, docker_app_params, "docker_app_params");
  writeOption(out_stream, docker_app_bin, "docker_app_bin");
  writeOption(out_stream, docker_compose_bin, "docker_compose_bin");
#endif
}
