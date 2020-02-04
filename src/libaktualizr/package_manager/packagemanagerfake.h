#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "package_manager/packagemanagerinterface.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  PackageManagerFake(PackageConfig pconfig, BootloaderConfig bconfig, std::shared_ptr<INvStorage> storage,
                     std::shared_ptr<HttpInterface> http)
      : PackageManagerInterface(std::move(pconfig), bconfig, std::move(storage), std::move(http)),
        bootloader_{new Bootloader(std::move(bconfig), *storage_)} {}
  ~PackageManagerFake() override = default;
  std::string name() const override { return "fake"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;

  data::InstallationResult install(const Uptane::Target &target) const override;
  void completeInstall() const override;
  data::InstallationResult finalizeInstall(const Uptane::Target &target) override;
  void updateNotify() override { bootloader_->updateNotify(); };
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) override;

 private:
  std::unique_ptr<Bootloader> bootloader_;
};

#endif  // PACKAGEMANAGERFAKE_H_
