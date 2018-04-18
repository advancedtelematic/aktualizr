#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "invstorage.h"
#include "package_manager/packagemanagerinterface.h"
#include "utilities/crypto.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  PackageManagerFake(const std::shared_ptr<INvStorage> &storage) : storage_(storage) {}
  Json::Value getInstalledPackages() override;

  Uptane::Target getCurrent() override;

  data::InstallOutcome install(const Uptane::Target &target) override;

 private:
  const std::shared_ptr<INvStorage> &storage_;
};

#endif  // PACKAGEMANAGERFAKE_H_
