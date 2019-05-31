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
    const PackageConfig& pconfig, const std::shared_ptr<INvStorage>& storage,
    const std::shared_ptr<Bootloader>& bootloader, const std::shared_ptr<HttpInterface>& http) {
  (void)bootloader;
  switch (pconfig.type) {
    case PackageManager::kOstree:
#ifdef BUILD_OSTREE
      return std::make_shared<OstreeManager>(pconfig, storage, bootloader, http);
#else
      throw std::runtime_error("aktualizr was compiled without OStree support!");
#endif
    case PackageManager::kDebian:
#ifdef BUILD_DEB
      return std::make_shared<DebianManager>(pconfig, storage, bootloader, http);
#else
      throw std::runtime_error("aktualizr was compiled without debian packages support!");
#endif
    case PackageManager::kAndroid:
#if defined(ANDROID)
      return std::make_shared<AndroidManager>(pconfig, storage, bootloader, http);
#else
      throw std::runtime_error("aktualizr was compiled without android support!");
#endif
    case PackageManager::kOstreeDockerApp:
#if defined(BUILD_DOCKERAPP) && defined(BUILD_OSTREE)
      return std::make_shared<DockerAppManager>(pconfig, storage, bootloader, http);
#else
      throw std::runtime_error("aktualizr was compiled without ostree+docker-app support!");
#endif
    case PackageManager::kNone:
      return std::make_shared<PackageManagerFake>(pconfig, storage, bootloader, http);
    default:
      LOG_ERROR << "Unrecognized package manager type: " << static_cast<int>(pconfig.type);
      return std::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
  }
}
