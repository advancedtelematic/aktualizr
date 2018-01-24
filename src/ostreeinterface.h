#ifndef OSTREEINTERFACE_H_
#define OSTREEINTERFACE_H_

#include <string>

#include <glib/gi18n.h>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "packagemanagerinterface.h"
#include "types.h"

class OstreePackageInterface : public PackageInterface {
 public:
  OstreePackageInterface(const std::string &ref_name_in, const std::string &refhash_in, const std::string &treehub_in)
      : PackageInterface(ref_name_in, refhash_in, treehub_in) {}
  virtual ~OstreePackageInterface() {}
  virtual data::InstallOutcome install(const data::PackageManagerCredentials &cred,
                                       const OstreeConfig &config) const = 0;
  virtual Json::Value toEcuVersion(const std::string &ecu_serial, const Json::Value &custom) const = 0;
};

class OstreeManagerInterface : public PackageManagerInterface {
 public:
  virtual Json::Value getInstalledPackages(const boost::filesystem::path &file_path) = 0;
  virtual std::string getCurrent(const boost::filesystem::path &ostree_sysroot) = 0;
  virtual boost::shared_ptr<PackageInterface> makePackage(const std::string &branch_name_in,
                                                          const std::string &refhash_in,
                                                          const std::string &treehub_in) = 0;
};

#endif  // OSTREEINTERFACE_H_
