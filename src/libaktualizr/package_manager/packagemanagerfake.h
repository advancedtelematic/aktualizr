#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "crypto/crypto.h"
#include "package_manager/packagemanagerinterface.h"

class PackageManagerFake : public PackageManagerBase {
 public:
  PackageManagerFake(PackageConfig pconfig, std::shared_ptr<INvStorage> storage, std::shared_ptr<Bootloader> bootloader,
                     std::shared_ptr<HttpInterface> http)
      : PackageManagerBase(pconfig, storage, bootloader, http) {}
  ~PackageManagerFake() override = default;
  std::string name() const override { return "fake"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;
  bool imageUpdated() override { return true; };

  data::InstallationResult install(const Uptane::Target &target) const override;
  void completeInstall() const override;
  data::InstallationResult finalizeInstall(const Uptane::Target &target) const override;
};

#endif  // PACKAGEMANAGERFAKE_H_
