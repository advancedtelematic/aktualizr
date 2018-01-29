#ifndef PACKAGEMANAGERFACTORY_H_
#define PACKAGEMANAGERFACTORY_H_

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "logging.h"
#include "ostree.h"
#include "ostreefake.h"
#include "packagemanagerfake.h"
#include "packagemanagerinterface.h"

class PackageManagerFactory {
 public:
  static boost::shared_ptr<PackageManagerInterface> makePackageManager(const PackageConfig& pconfig) {
    switch (pconfig.type) {
      case kOstree:
        return boost::make_shared<OstreeManager>(pconfig);
      case kOstreeFake:
        return boost::make_shared<OstreeFakeManager>();
      case kDebian:
        LOG_ERROR << "Debian package managament is not currently supported.";
        return boost::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
      case kNone:
        return boost::make_shared<OstreeFakeManager>();
      default:
        LOG_ERROR << "Unrecognized package manager type: " << pconfig.type;
        return boost::shared_ptr<PackageManagerInterface>();  // NULL-equivalent
    }
  }
};

#endif  // PACKAGEMANAGERFACTORY_H_
