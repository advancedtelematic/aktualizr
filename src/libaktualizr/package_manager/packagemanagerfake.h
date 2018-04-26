#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "crypto/crypto.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  explicit PackageManagerFake(const std::shared_ptr<INvStorage> &storage) : storage_(storage) {}
  std::string name() override { return "fake"; }
  Json::Value getInstalledPackages() override;

  Uptane::Target getCurrent() override;

  data::InstallOutcome install(const Uptane::Target &target) const override;

 private:
  const std::shared_ptr<INvStorage> &storage_;
};

#endif  // PACKAGEMANAGERFAKE_H_
