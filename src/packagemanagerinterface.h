#ifndef PACKAGEMANAGERINTERFACE_H_
#define PACKAGEMANAGERINTERFACE_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include "config.h"
#include "types.h"

class PackageInterface {
 public:
  PackageInterface(const std::string &ref_name_in, const std::string &refhash_in, const std::string &treehub_in)
      : ref_name(ref_name_in), refhash(refhash_in), pull_uri(treehub_in) {}
  virtual ~PackageInterface() {}
  virtual data::InstallOutcome install(const data::PackageManagerCredentials &cred,
                                       const PackageConfig &pconfig) const = 0;

 protected:
  std::string ref_name;
  std::string refhash;
  std::string pull_uri;
};

class PackageManagerInterface {
 public:
  virtual Json::Value getInstalledPackages() = 0;
  virtual std::string getCurrent() = 0;
  virtual boost::shared_ptr<PackageInterface> makePackage(const std::string &branch_name_in,
                                                          const std::string &refhash_in,
                                                          const std::string &treehub_in) = 0;
};

#endif  // PACKAGEMANAGERINTERFACE_H_
