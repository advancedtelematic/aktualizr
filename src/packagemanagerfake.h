#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "crypto.h"
#include "packagemanagerinterface.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  virtual Json::Value getInstalledPackages() {
    Json::Value packages(Json::arrayValue);
    Json::Value package;
    package["name"] = "fake-package";
    package["version"] = "1.0";
    packages.append(package);
    return packages;
  }

  virtual std::string getCurrent() { return refhash_fake; }

  virtual data::InstallOutcome install(const Uptane::Target &target) const {
    (void)target;
    return data::InstallOutcome(data::OK, "Pulling ostree image was successful");
  };
};

#endif  // PACKAGEMANAGERFAKE_H_
