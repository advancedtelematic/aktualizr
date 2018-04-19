#include "package_manager/debianmanager.h"

#define LIBDPKG_VOLATILE_API 1
#include <dpkg/dpkg-db.h>
#include <dpkg/dpkg.h>
#include <dpkg/pkg-array.h>
#include <dpkg/pkg-show.h>
#include <stdio.h>
#include <unistd.h>

Json::Value DebianManager::getInstalledPackages() {
  Json::Value packages(Json::arrayValue);
  struct pkg_array array;
  dpkg_program_init("a.out");
  modstatdb_open(msdbrw_available_readonly);

  pkg_array_init_from_db(&array);
  pkg_array_sort(&array, pkg_sorter_by_nonambig_name_arch);
  for (int i = 0; i < array.n_pkgs; ++i) {
    if (array.pkgs[i]->status == PKG_STAT_INSTALLED) {
      Json::Value package;
      package["name"] = pkg_name(array.pkgs[i], pnaw_nonambig);
      package["version"] = versiondescribe(&array.pkgs[i]->installed.version, vdew_nonambig);
      packages.append(package);
    }
  }
  dpkg_program_done();
  return packages;
}

data::InstallOutcome DebianManager::install(const Uptane::Target &target) const {
  LOG_INFO << "Installing " << target.filename() << " as Debian package...";
  std::string cmd = "dpkg -i ";
  std::string output;
  TemporaryDirectory package_dir("deb_dir");
  std::stringstream sstr;
  sstr << *storage_->openTargetFile(target.filename());
  boost::filesystem::path deb_path = package_dir / target.filename();
  Utils::writeFile(deb_path, sstr.str());

  int status = Utils::shell(cmd + deb_path.string(), &output, true);
  if (status == 0) {
    LOG_INFO << "... Installation of Debian package successful";
    storage_->saveInstalledVersion(target);
    return data::InstallOutcome(data::OK, "Installing debian package was successful");
  } else {
    LOG_ERROR << "... Installation of Debian package failed";
    return data::InstallOutcome(data::INSTALL_FAILED, output);
  }
}

Uptane::Target DebianManager::getCurrent() {
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