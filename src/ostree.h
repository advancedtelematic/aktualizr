#ifndef OSTREE_H_
#define OSTREE_H_

#include <string>

#include <glib/gi18n.h>
#include <ostree-1/ostree.h>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "packagemanagerinterface.h"
#include "types.h"
#include "uptane/cryptokey.h"
const char remote[] = "aktualizr-remote";

class OstreeManager : public PackageManagerInterface {
 public:
  OstreeManager(const PackageConfig &pconfig);
  virtual Json::Value getInstalledPackages();
  virtual std::string getCurrent();
  virtual data::InstallOutcome install(const Uptane::Target &target) const;

  boost::shared_ptr<OstreeDeployment> getStagedDeployment();
  static boost::shared_ptr<OstreeSysroot> LoadSysroot(const boost::filesystem::path &path);
  static bool addRemote(OstreeRepo *repo, const std::string &url, const data::PackageManagerCredentials &cred);
  static data::InstallOutcome pull(const Config &config, const data::PackageManagerCredentials &cred,
                                   const std::string &refhash);

 private:
  PackageConfig config;
};

#endif  // OSTREE_H_
