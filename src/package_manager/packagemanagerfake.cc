#include "packagemanagerfake.h"

Json::Value PackageManagerFake::getInstalledPackages() {
  Json::Value packages(Json::arrayValue);
  Json::Value package;
  package["name"] = "fake-package";
  package["version"] = "1.0";
  packages.append(package);
  return packages;
}

Uptane::Target PackageManagerFake::getCurrent() {
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

data::InstallOutcome PackageManagerFake::install(const Uptane::Target &target) const {
  storage_->saveInstalledVersion(target);
  return data::InstallOutcome(data::OK, "Installing fake package was successful");
};
