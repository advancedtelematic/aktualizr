#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerfake.h"

#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"
#ifdef BUILD_DOCKERAPP
#include "package_manager/dockerappmanager.h"
#endif
#endif

#ifdef BUILD_DEB
#include "package_manager/debianmanager.h"
#endif

#if defined(ANDROID)
#include "package_manager/androidmanager.h"
#endif

#include "logging/logging.h"

std::shared_ptr<PackageManagerInterface> PackageManagerFactory::makePackageManager(
    const PackageConfig& pconfig, const BootloaderConfig& bconfig, const std::shared_ptr<INvStorage>& storage,
    const std::shared_ptr<HttpInterface>& http) {
  if (pconfig.type == PACKAGE_MANAGER_OSTREE) {
#ifdef BUILD_OSTREE
      return std::make_shared<OstreeManager>(pconfig, bconfig, storage, http);
#else
      throw std::runtime_error("aktualizr was compiled without OStree support!");
#endif
  } else if (pconfig.type == PACKAGE_MANAGER_DEBIAN) {
#ifdef BUILD_DEB
      return std::make_shared<DebianManager>(pconfig, bconfig, storage, http);
#else
      throw std::runtime_error("aktualizr was compiled without debian packages support!");
#endif
  } else if (pconfig.type == PACKAGE_MANAGER_ANDROID) {
#if defined(ANDROID)
      return std::make_shared<AndroidManager>(pconfig, bconfig, storage, http);
#else
      throw std::runtime_error("aktualizr was compiled without android support!");
#endif
  } else if (pconfig.type == PACKAGE_MANAGER_OSTREEDOCKERAPP) {
#if defined(BUILD_DOCKERAPP) && defined(BUILD_OSTREE)
      return std::make_shared<DockerAppManager>(pconfig, bconfig, storage, http);
#else
      throw std::runtime_error("aktualizr was compiled without ostree+docker-app support!");
#endif
  } else if (pconfig.type == PACKAGE_MANAGER_NONE) {
      return std::make_shared<PackageManagerFake>(pconfig, bconfig, storage, http);
  } else {
      LOG_ERROR << "Unrecognized package manager type: " << pconfig.type;
      return std::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
  }
}
