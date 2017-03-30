#ifndef OSTREE_H_
#define OSTREE_H_

#include <string>

#include <glib/gi18n.h>
#include "ostree-1/ostree.h"
#include "types.h"

typedef std::pair<data::UpdateResultCode, std::string> InstallOutcome;
static const std::string NEW_PACKAGE = "/tmp/sota-package";
static const std::string BOOT_BRANCH = "/usr/share/sota/branchname";

class OstreePackage {
 public:
  OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in, const std::string &commit_in,
                const std::string &desc_in, const std::string &treehub_in);
  std::string ecu_serial;
  std::string ref_name;
  std::string commit;
  std::string description;
  std::string pull_uri;
  InstallOutcome install(const data::PackageManagerCredentials &cred);
  Json::Value toEcuVersion(const Json::Value &custom);
  static OstreePackage getEcu(const std::string &);
  static OstreePackage fromJson(const Json::Value &json);
};

struct OstreeBranch {
  OstreeBranch(bool current_in, const std::string &os_name_in, const OstreePackage &package_in)
      : current(current_in), os_name(os_name_in), package(package_in) {}
  static OstreeBranch getCurrent(const std::string &ecu_serial, const std::string &branch);
  bool current;
  std::string os_name;
  OstreePackage package;
};

#endif  // OSTREE_H_