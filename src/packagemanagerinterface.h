#ifndef PACKAGEMANAGERINTERFACE_H_
#define PACKAGEMANAGERINTERFACE_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include "config.h"
#include "types.h"

class PackageManagerInterface {
 public:
  virtual Json::Value getInstalledPackages() = 0;
  virtual std::string getCurrent() = 0;
  virtual data::InstallOutcome install(const Uptane::Target &target) const = 0;
};

#endif  // PACKAGEMANAGERINTERFACE_H_
