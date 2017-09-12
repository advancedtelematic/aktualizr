#ifndef OSTREE_H_
#define OSTREE_H_

#include <glib/gi18n.h>
#include <ostree-1/ostree.h>
#include <boost/shared_ptr.hpp>
#include <string>
#include "config.h"
#include "types.h"

struct Ostree {
  static boost::shared_ptr<OstreeDeployment> getStagedDeployment(const std::string &path);
  static boost::shared_ptr<OstreeSysroot> LoadSysroot(const std::string &path);
  static bool addRemote(OstreeRepo *repo, const std::string &remote, const std::string &url,
                        const data::PackageManagerCredentials &cred);
  static Json::Value getInstalledPackages(const std::string &file_path);
};

class OstreePackage {
 public:
  OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in, const std::string &branch_name_in,
                const std::string &refhash_in, const std::string &desc_in, const std::string &treehub_in);
  std::string ecu_serial;
  std::string ref_name;
  std::string branch_name;
  std::string refhash;
  std::string description;
  std::string pull_uri;
  data::InstallOutcome install(const data::PackageManagerCredentials &cred, OstreeConfig config) const;

  Json::Value toEcuVersion(const Json::Value &custom) const;
  static OstreePackage getEcu(const std::string &ecu_serial, const std::string &ostree_sysroot);
};

struct OstreeBranch {
  OstreeBranch(bool current_in, const std::string &os_name_in, const OstreePackage &package_in)
      : current(current_in), os_name(os_name_in), package(package_in) {}
  static OstreeBranch getCurrent(const std::string &ecu_serial, const std::string &ostree_sysroot);
  bool current;
  std::string os_name;
  OstreePackage package;
};

#endif  // OSTREE_H_
