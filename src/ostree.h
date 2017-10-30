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
  OstreePackage(const std::string &branch_name_in, const std::string &refhash_in, const std::string &treehub_in);
  std::string ref_name;
  std::string refhash;
  std::string pull_uri;
  data::InstallOutcome install(const data::PackageManagerCredentials &cred, OstreeConfig config,
                               const std::string &refspec) const;

  Json::Value toEcuVersion(const std::string &ecu_serial, const Json::Value &custom) const;
  static std::string getCurrent(const std::string &ostree_sysroot);
};

#endif  // OSTREE_H_
