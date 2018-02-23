#ifndef OSTREE_H_
#define OSTREE_H_

#include <string>

#include <glib/gi18n.h>
#include <ostree-1/ostree.h>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "keymanager.h"
#include "packagemanagerinterface.h"
#include "types.h"

const char remote[] = "aktualizr-remote";

class OstreeManager : public PackageManagerInterface {
 public:
  OstreeManager(const PackageConfig &pconfig, const boost::shared_ptr<INvStorage> &storage);
  Json::Value getInstalledPackages() override;
  Uptane::Target getCurrent() override;
  data::InstallOutcome install(const Uptane::Target &target) const override;

  boost::shared_ptr<OstreeDeployment> getStagedDeployment();
  static boost::shared_ptr<OstreeSysroot> LoadSysroot(const boost::filesystem::path &path);
  static bool addRemote(OstreeRepo *repo, const std::string &url, const KeyManager &keys);
  static data::InstallOutcome pull(const Config &config, const KeyManager &keys, const std::string &refhash);

 private:
  PackageConfig config;
  boost::shared_ptr<INvStorage> storage_;
};

#endif  // OSTREE_H_
