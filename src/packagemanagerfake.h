#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "crypto.h"
#include "packagemanagerinterface.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  PackageManagerFake(const std::shared_ptr<INvStorage> &storage) : storage_(storage) {}
  Json::Value getInstalledPackages() override {
    Json::Value packages(Json::arrayValue);
    Json::Value package;
    package["name"] = "fake-package";
    package["version"] = "1.0";
    packages.append(package);
    return packages;
  }

  Uptane::Target getCurrent() override {
    std::vector<Uptane::Target> installed_versions;
    std::string current_hash = storage_->loadInstalledVersions(&installed_versions);

    std::vector<Uptane::Target>::iterator it;
    for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
      if (it->sha256Hash() == current_hash) {
        return *it;
      }
    }
    return getUnknown();
  }

  data::InstallOutcome install(const Uptane::Target &target) const override {
    storage_->saveInstalledVersion(target);
    return data::InstallOutcome(data::OK, "Installing fake package was successful");
  };

 private:
  const std::shared_ptr<INvStorage> &storage_;
};

#endif  // PACKAGEMANAGERFAKE_H_
