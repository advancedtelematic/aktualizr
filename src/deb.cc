#include "deb.h"

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

  // Install the new package. This is complicated by the fact that it needs to
  // support self-update, where aktualizr installs a new aktualzr Debian
  // package.  Removing the old package stops the Aktualizr systemd service,
  // which causes Aktualizr to get killed. Therefore we perform the
  // installation in a separate cgroup, using systemd-run.
  // This is a temporary solution--in the future we should juggle things to
  // report the installation status correctly.
  const std::string tmp_deb_file("/tmp/incoming-package.deb", std::ofstream::trunc);  // TODO Make this secure
  std::ofstream package(tmp_deb_file);
  package << *storage_->openTargetFile(target.filename());
  package.close();

  std::string cmd = "systemd-run --no-ask-password dpkg -i " + tmp_deb_file;

  std::string output;
  int status = Utils::shell(cmd, &output, true);
  if (status == 0) {
    // This is only checking the result of the systemd-run command
    LOG_INFO << "... Installation of Debian package successful";
    storage_->saveInstalledVersion(target);
    return data::InstallOutcome(data::OK, "Installing debian package was successful");
  } else {
    LOG_WARNING << "... Installation of Debian package failed";
    return data::InstallOutcome(data::INSTALL_FAILED, output);
  }
}

std::string DebianManager::getCurrent() {
  std::map<std::string, InstalledVersion> installed_versions;
  storage_->loadInstalledVersions(&installed_versions);

  std::map<std::string, InstalledVersion>::iterator it;
  for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
    if ((*it).second.second) {
      return it->first;
    }
  }
  return boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
}