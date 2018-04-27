#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerfake.h"

#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"
#endif

#ifdef BUILD_DEB
#include "package_manager/debianmanager.h"
#endif

#include "logging/logging.h"

std::shared_ptr<PackageManagerInterface> PackageManagerFactory::makePackageManager(
    const PackageConfig& pconfig, const std::shared_ptr<INvStorage>& storage) {
  (void)storage;
  switch (pconfig.type) {
    case kOstree:
#ifdef BUILD_OSTREE
      return std::make_shared<OstreeManager>(pconfig, storage);
#else
      throw std::runtime_error("aktualizr was compiled without OStree support!");
#endif
    case kDebian:
#ifdef BUILD_DEB
      return std::make_shared<DebianManager>(pconfig, storage);
#else
      throw std::runtime_error("aktualizr was compiled without debian packages support!");
#endif
    case kNone:
      return std::make_shared<PackageManagerFake>(storage);
    default:
      LOG_ERROR << "Unrecognized package manager type: " << pconfig.type;
      return std::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
  }
}
