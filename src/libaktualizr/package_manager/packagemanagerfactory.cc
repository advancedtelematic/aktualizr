#include <cstdlib>
#include <map>

#include "libaktualizr/packagemanagerfactory.h"

#include "logging/logging.h"

static std::map<std::string, PackageManagerBuilder> *registered_pkgms_;

bool PackageManagerFactory::registerPackageManager(const char *name, PackageManagerBuilder builder) {
  // this trick is needed so that this object is constructed before any element
  // is added to it
  static std::map<std::string, PackageManagerBuilder> rpkgms;

  if (registered_pkgms_ == nullptr) {
    registered_pkgms_ = &rpkgms;
  }
  if (registered_pkgms_->find(name) != registered_pkgms_->end()) {
    throw std::runtime_error(std::string("fatal: tried to register package manager \"") + name + "\" twice");
  }
  (*registered_pkgms_)[name] = std::move(builder);
  return true;
}

std::shared_ptr<PackageManagerInterface> PackageManagerFactory::makePackageManager(
    const PackageConfig &pconfig, const BootloaderConfig &bconfig, const std::shared_ptr<INvStorage> &storage,
    const std::shared_ptr<HttpInterface> &http) {
  for (const auto &b : *registered_pkgms_) {
    if (b.first == pconfig.type) {
      PackageManagerInterface *pkgm = b.second(pconfig, bconfig, storage, http);
      return std::shared_ptr<PackageManagerInterface>(pkgm);
    }
  }

  LOG_ERROR << "Package manager type \"" << pconfig.type << "\" does not exist";
  LOG_ERROR << "Available options are: " << []() {
    std::stringstream ss;
    for (const auto &b : *registered_pkgms_) {
      ss << "\n" << b.first;
    }
    return ss.str();
  }();

  throw std::runtime_error(std::string("Unsupported package manager: ") + pconfig.type);
}
