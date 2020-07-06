#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/log/trivial.hpp>

#include "libaktualizr/config.h"
#include "utilities/config_utils.h"

void PackageConfig::updateFromPropertyTree(const boost::property_tree::ptree& pt) {
  for (const auto& cp : pt) {
    if (cp.first == "type") {
      CopyFromConfig(type, cp.first, pt);
    } else if (cp.first == "os") {
      CopyFromConfig(os, cp.first, pt);
    } else if (cp.first == "sysroot") {
      CopyFromConfig(sysroot, cp.first, pt);
    } else if (cp.first == "ostree_server") {
      CopyFromConfig(ostree_server, cp.first, pt);
    } else if (cp.first == "images_path") {
      CopyFromConfig(images_path, cp.first, pt);
    } else if (cp.first == "packages_file") {
      CopyFromConfig(packages_file, cp.first, pt);
    } else if (cp.first == "fake_need_reboot") {
      CopyFromConfig(fake_need_reboot, cp.first, pt);
    } else {
      extra[cp.first] = Utils::stripQuotes(cp.second.get_value<std::string>());
    }
  }
}

void PackageConfig::writeToStream(std::ostream& out_stream) const {
  writeOption(out_stream, type, "type");
  writeOption(out_stream, os, "os");
  writeOption(out_stream, sysroot, "sysroot");
  writeOption(out_stream, ostree_server, "ostree_server");
  writeOption(out_stream, images_path, "images_path");
  writeOption(out_stream, packages_file, "packages_file");
  writeOption(out_stream, fake_need_reboot, "fake_need_reboot");

  // note that this is imperfect as it will not print default values deduced
  // from users of `extra`
  for (const auto& e : extra) {
    writeOption(out_stream, e.first, e.second);
  }
}
