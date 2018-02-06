#ifndef DEB_H_
#define DEB_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include "config.h"
#include "packagemanagerinterface.h"
#include "types.h"

class DebianManager : public PackageManagerInterface {
 public:
  DebianManager(const PackageConfig &pconfig, const boost::filesystem::path &path) : config_(pconfig), path_(path) {}
  virtual Json::Value getInstalledPackages();
  virtual std::string getCurrent();
  virtual data::InstallOutcome install(const Uptane::Target &target) const;
  PackageConfig config_;
  boost::filesystem::path path_;
};

#endif  // DEB_H_
