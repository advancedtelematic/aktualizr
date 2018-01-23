#ifndef OSTREE_H_
#define OSTREE_H_

#include <string>

#include <glib/gi18n.h>
#include <ostree-1/ostree.h>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "ostreeinterface.h"
#include "types.h"
#include "uptane/cryptokey.h"

class OstreePackage : public OstreePackageInterface {
 public:
  OstreePackage(const std::string &ref_name_in, const std::string &refhash_in, const std::string &treehub_in);
  virtual ~OstreePackage() {}
  virtual data::InstallOutcome install(const data::PackageManagerCredentials &cred, const OstreeConfig &config) const;
  virtual Json::Value toEcuVersion(const std::string &ecu_serial, const Json::Value &custom) const;
};

class OstreeManager : public OstreeManagerInterface {
 public:
  static boost::shared_ptr<OstreeDeployment> getStagedDeployment(const boost::filesystem::path &path);
  static boost::shared_ptr<OstreeSysroot> LoadSysroot(const boost::filesystem::path &path);
  static bool addRemote(OstreeRepo *repo, const std::string &remote, const std::string &url,
                        const data::PackageManagerCredentials &cred);
  virtual Json::Value getInstalledPackages(const boost::filesystem::path &file_path);  // could be static
  virtual std::string getCurrent(const boost::filesystem::path &ostree_sysroot);       // could be static
  virtual boost::shared_ptr<PackageInterface> makePackage(const std::string &branch_name_in,
                                                          const std::string &refhash_in, const std::string &treehub_in);
};

#endif  // OSTREE_H_
