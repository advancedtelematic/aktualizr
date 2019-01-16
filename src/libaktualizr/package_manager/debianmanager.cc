#include "package_manager/debianmanager.h"

#define LIBDPKG_VOLATILE_API 1
#include <dpkg/dpkg-db.h>
#include <dpkg/dpkg.h>
#include <dpkg/pkg-array.h>
#include <dpkg/pkg-show.h>
#include <stdio.h>
#include <unistd.h>

Json::Value DebianManager::getInstalledPackages() const {
  Json::Value packages(Json::arrayValue);
  struct pkg_array array {};
  dpkg_program_init("a.out");
  modstatdb_open(msdbrw_available_readonly);

  pkg_array_init_from_db(&array);
  pkg_array_sort(&array, pkg_sorter_by_nonambig_name_arch);
  for (int i = 0; i < array.n_pkgs; ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    struct pkginfo *pkg = array.pkgs[i];
    if (pkg->status == PKG_STAT_INSTALLED) {
      Json::Value package;
      package["name"] = pkg_name(pkg, pnaw_nonambig);
      package["version"] = versiondescribe(&pkg->installed.version, vdew_nonambig);
      packages.append(package);
    }
  }
  dpkg_program_done();
  return packages;
}

data::InstallOutcome DebianManager::install(const Uptane::Target &target) const {
  std::lock_guard<std::mutex> guard(mutex_);
  LOG_INFO << "Installing " << target.filename() << " as Debian package...";
  std::string cmd = "dpkg -i ";
  std::string output;
  TemporaryDirectory package_dir("deb_dir");
  auto target_file = storage_->openTargetFile(target);

  boost::filesystem::path deb_path = package_dir / target.filename();
  target_file->writeToFile(deb_path);
  target_file->rclose();

  int status = Utils::shell(cmd + deb_path.string(), &output, true);
  if (status == 0) {
    LOG_INFO << "... Installation of Debian package successful";
    storage_->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kCurrent);
    return data::InstallOutcome(data::UpdateResultCode::kOk, "Installing debian package was successful");
  }
  LOG_ERROR << "... Installation of Debian package failed";
  return data::InstallOutcome(data::UpdateResultCode::kInstallFailed, output);
}

Uptane::Target DebianManager::getCurrent() const {
  std::vector<Uptane::Target> installed_versions;
  size_t current_k = SIZE_MAX;
  storage_->loadPrimaryInstalledVersions(&installed_versions, &current_k, nullptr);

  if (current_k != SIZE_MAX) {
    return installed_versions.at(current_k);
  }

  return Uptane::Target::Unknown();
}
