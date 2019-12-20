#ifndef PACKAGEMANAGERFACTORY_H_
#define PACKAGEMANAGERFACTORY_H_

#include "config/config.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"

using PackageManagerBuilder =
    std::function<PackageManagerInterface*(const PackageConfig&, const BootloaderConfig&,
                                           const std::shared_ptr<INvStorage>&, const std::shared_ptr<HttpInterface>&)>;

class PackageManagerFactory {
 public:
  static bool registerPackageManager(const char* name, PackageManagerBuilder builder);
  static std::shared_ptr<PackageManagerInterface> makePackageManager(const PackageConfig& pconfig,
                                                                     const BootloaderConfig& bconfig,
                                                                     const std::shared_ptr<INvStorage>& storage,
                                                                     const std::shared_ptr<HttpInterface>& http);
};

// macro to auto-register a package manager
// note that static library users will have to call `registerPackageManager` manually

#define AUTO_REGISTER_PACKAGE_MANAGER(name, clsname)                                                         \
  class clsname##_PkgMRegister_ {                                                                            \
   public:                                                                                                   \
    clsname##_PkgMRegister_() {                                                                              \
      PackageManagerFactory::registerPackageManager(                                                         \
          name, [](const PackageConfig& pconfig, const BootloaderConfig& bconfig,                            \
                   const std::shared_ptr<INvStorage>& storage, const std::shared_ptr<HttpInterface>& http) { \
            return new clsname(pconfig, bconfig, storage, http);                                             \
          });                                                                                                \
    }                                                                                                        \
  };                                                                                                         \
  static clsname##_PkgMRegister_ clsname##_register_

#endif  // PACKAGEMANAGERFACTORY_H_
