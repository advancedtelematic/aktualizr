#ifndef PACKAGEMANAGERFACTORY_H_
#define PACKAGEMANAGERFACTORY_H_

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "invstorage.h"
#include "logging.h"
#include "packagemanagerfake.h"
#include "packagemanagerinterface.h"

#ifdef BUILD_OSTREE
#include "ostree.h"
#endif

#ifdef BUILD_DEB
#include "deb.h"
#endif

class PackageManagerFactory {
 public:
  static boost::shared_ptr<PackageManagerInterface> makePackageManager(const PackageConfig& pconfig,
                                                                       const boost::shared_ptr<INvStorage>& storage) {
    (void)storage;
    switch (pconfig.type) {
      case kOstree:
#ifdef BUILD_OSTREE
        return boost::make_shared<OstreeManager>(pconfig, storage);
#else
        throw std::runtime_error("aktualizr was compiled without OStree support!");
#endif
      case kDebian:
#ifdef BUILD_DEB
        return boost::make_shared<DebianManager>(pconfig, storage);
#else
        throw std::runtime_error("aktualizr was compiled without debian packages support!");
#endif
      case kNone:
        return boost::make_shared<PackageManagerFake>();
      default:
        LOG_ERROR << "Unrecognized package manager type: " << pconfig.type;
        return boost::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
    }
  }
};

#endif  // PACKAGEMANAGERFACTORY_H_
