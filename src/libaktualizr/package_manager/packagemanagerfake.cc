#include "packagemanagerfake.h"

Json::Value PackageManagerFake::getInstalledPackages() const {
  Json::Value packages(Json::arrayValue);
  Json::Value package;
  package["name"] = "fake-package";
  package["version"] = "1.0";
  packages.append(package);
  return packages;
}

Uptane::Target PackageManagerFake::getCurrent() const {
  std::vector<Uptane::Target> installed_versions;
  size_t current_k = SIZE_MAX;
  storage_->loadPrimaryInstalledVersions(&installed_versions, &current_k, nullptr);

  if (current_k != SIZE_MAX) {
    return installed_versions.at(current_k);
  }

  return getUnknown();
}

data::InstallOutcome PackageManagerFake::install(const Uptane::Target &target) const {
  storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kCurrent);
  return data::InstallOutcome(data::UpdateResultCode::kOk, "Installing fake package was successful");
}
