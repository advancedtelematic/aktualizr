#ifndef OSTREE_H_
#define OSTREE_H_

#include <string>

#include <glib/gi18n.h>
#include <ostree-1/ostree.h>

#include "config/config.h"
#include "crypto/keymanager.h"
#include "package_manager/packagemanagerinterface.h"
#include "utilities/types.h"

const char remote[] = "aktualizr-remote";

class OstreeManager : public PackageManagerInterface {
 public:
  OstreeManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage);
  std::string name() override { return "ostree"; }
  Json::Value getInstalledPackages() override;
  Uptane::Target getCurrent() override;
  data::InstallOutcome install(const Uptane::Target &target) const override;

  std::shared_ptr<OstreeDeployment> getStagedDeployment();
  static std::shared_ptr<OstreeSysroot> LoadSysroot(const boost::filesystem::path &path);
  static std::shared_ptr<OstreeRepo> LoadRepo(const std::shared_ptr<OstreeSysroot> &sysroot, GError **error);
  static bool addRemote(OstreeRepo *repo, const std::string &url, const KeyManager &keys);
  static data::InstallOutcome pull(const boost::filesystem::path &sysroot_path, const std::string &ostree_server,
                                   const KeyManager &keys, const std::string &refhash);

 private:
  PackageConfig config;
  std::shared_ptr<INvStorage> storage_;
};

#endif  // OSTREE_H_
