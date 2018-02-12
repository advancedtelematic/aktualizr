#ifndef DEB_H_
#define DEB_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include "config.h"
#include "packagemanagerinterface.h"
#include "types.h"

class DebianManager : public PackageManagerInterface {
 public:
  DebianManager(const PackageConfig &pconfig) : config_(pconfig) {}
  virtual Json::Value getInstalledPackages();
  virtual std::string getCurrent() {
    throw std::runtime_error("Not implemented yet");
    return "";
  }
  virtual data::InstallOutcome install(const Uptane::Target &target) const {
    (void)target;
    throw std::runtime_error("Not implemented yet");
    return data::InstallOutcome(data::OK, "Pulling ostree image was successful");
  }
  PackageConfig config_;
};

#endif  // DEB_H_
