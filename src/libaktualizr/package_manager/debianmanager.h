#ifndef DEB_H_
#define DEB_H_

#include <string>
#include <utility>

#include "config.h"
#include "package_manager/packagemanagerinterface.h"
#include "utilities/types.h"

#include "storage/invstorage.h"

class DebianManager : public PackageManagerInterface {
 public:
  DebianManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage)
      : config_(std::move(pconfig)), storage_(std::move(storage)) {}
  std::string name() override { return "debian"; }
  Json::Value getInstalledPackages() override;
  Uptane::Target getCurrent() override;
  data::InstallOutcome install(const Uptane::Target &target) const override;
  PackageConfig config_;
  std::shared_ptr<INvStorage> storage_;
};

#endif  // DEB_H_
