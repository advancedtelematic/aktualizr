#ifndef DEB_H_
#define DEB_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include "config.h"
#include "packagemanagerinterface.h"
#include "types.h"

#include "invstorage.h"

class DebianManager : public PackageManagerInterface {
 public:
  DebianManager(const PackageConfig &pconfig, const boost::shared_ptr<INvStorage> &storage)
      : config_(pconfig), storage_(storage) {}
  Json::Value getInstalledPackages() override;
  Uptane::Target getCurrent() override;
  data::InstallOutcome install(const Uptane::Target &target) const override;
  PackageConfig config_;
  boost::shared_ptr<INvStorage> storage_;
};

#endif  // DEB_H_
