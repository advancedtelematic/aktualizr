#ifndef PACKAGEMANAGERFACTORY_H_
#define PACKAGEMANAGERFACTORY_H_

#include "config.h"
#include "invstorage.h"
#include "package_manager/packagemanagerinterface.h"

class PackageManagerFactory {
 public:
  static std::shared_ptr<PackageManagerInterface> makePackageManager(const PackageConfig& pconfig,
                                                                     const std::shared_ptr<INvStorage>& storage);
};

#endif  // PACKAGEMANAGERFACTORY_H_
