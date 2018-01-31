#ifndef PACKAGEMANAGERFACTORY_H_
#define PACKAGEMANAGERFACTORY_H_

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "logging.h"
#include "packagemanagerfake.h"
#include "packagemanagerinterface.h"

#ifdef BUILD_OSTREE
#include "ostree.h"
#endif

class PackageManagerFactory {
 public:
  static boost::shared_ptr<PackageManagerInterface> makePackageManager(const PackageConfig& pconfig) {
    switch (pconfig.type) {
      case kOstree:
#ifdef BUILD_OSTREE
        return boost::make_shared<OstreeManager>(pconfig);
#else
        throw std::runtime_error("aktualizr was compiled without OStree support!");
#endif
      case kDebian:
        LOG_ERROR << "Debian package managament is not currently supported.";
        return boost::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
      case kNone:
        return boost::make_shared<PackageManagerFake>();
      default:
        LOG_ERROR << "Unrecognized package manager type: " << pconfig.type;
        return boost::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
    }
  }
};

#endif  // PACKAGEMANAGERFACTORY_H_
