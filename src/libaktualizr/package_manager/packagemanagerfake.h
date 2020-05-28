#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "package_manager/packagemanagerinterface.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  PackageManagerFake(const PackageConfig &pconfig, const BootloaderConfig &bconfig,
                     const std::shared_ptr<INvStorage> &storage, const std::shared_ptr<HttpInterface> &http)
      : PackageManagerInterface(pconfig, bconfig, storage, http), bootloader_{new Bootloader(bconfig, *storage_)} {}
  ~PackageManagerFake() override = default;
  std::string name() const override { return "fake"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;

  data::InstallationResult install(const Uptane::Target &target) const override;
  void completeInstall() const override;
  data::InstallationResult finalizeInstall(const Uptane::Target &target) override;
  void updateNotify() override { bootloader_->updateNotify(); };
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   const FetcherProgressCb &progress_cb, const api::FlowControlToken *token) override;

 private:
  std::unique_ptr<Bootloader> bootloader_;
};

#endif  // PACKAGEMANAGERFAKE_H_
