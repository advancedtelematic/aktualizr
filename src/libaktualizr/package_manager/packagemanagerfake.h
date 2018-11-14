#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "crypto/crypto.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  explicit PackageManagerFake(std::shared_ptr<INvStorage> storage) : storage_(std::move(storage)) {}
  ~PackageManagerFake() override = default;
  std::string name() const override { return "fake"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;
  bool imageUpdated() override { return true; };

  data::InstallOutcome install(const Uptane::Target &target) const override;
  data::InstallOutcome finalizeInstall(const Uptane::Target &target) const override {
    (void)target;
    throw std::runtime_error("Unimplemented");
  }

 private:
  std::shared_ptr<INvStorage> storage_;
};

#endif  // PACKAGEMANAGERFAKE_H_
