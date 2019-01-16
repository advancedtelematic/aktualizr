#include "packagemanagerfake.h"

#include "utilities/fault_injection.h"

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

  return Uptane::Target::Unknown();
}

data::InstallOutcome PackageManagerFake::install(const Uptane::Target &target) const {
  if (fiu_fail("fake_package_install")) {
    return data::InstallOutcome(data::UpdateResultCode::kInstallFailed, "Installation failed");
  }

  if (config.fake_need_reboot) {
    storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);

    // set reboot flag to be notified later
    if (bootloader_ != nullptr) {
      bootloader_->rebootFlagSet();
    }

    return data::InstallOutcome(data::UpdateResultCode::kNeedCompletion, "Application successful, need reboot");
  }

  storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kCurrent);
  return data::InstallOutcome(data::UpdateResultCode::kOk, "Installing fake package was successful");
}

data::InstallOutcome PackageManagerFake::finalizeInstall(const Uptane::Target &target) const {
  std::vector<Uptane::Target> targets;
  size_t pending_version = SIZE_MAX;
  storage_->loadPrimaryInstalledVersions(&targets, nullptr, &pending_version);

  if (pending_version == SIZE_MAX) {
    throw std::runtime_error("No pending update, nothing to finalize");
  }

  data::InstallOutcome outcome;
  if (target == targets[pending_version]) {
    outcome = data::InstallOutcome(data::UpdateResultCode::kOk, "Installing fake package was successful");
  } else {
    outcome = data::InstallOutcome(data::UpdateResultCode::kInternalError, "Pending and new target do not match");
  }

  storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kCurrent);
  return outcome;
}
