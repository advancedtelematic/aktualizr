#ifndef PACKAGEMANAGERFAKE_H_
#define PACKAGEMANAGERFAKE_H_

#include <memory>
#include <string>

#include "package_manager/packagemanagerinterface.h"

class PackageManagerFake : public PackageManagerInterface {
 public:
  PackageManagerFake(const PackageConfig &pconfig, const BootloaderConfig &bconfig,
                     const std::shared_ptr<INvStorage> &storage, const std::shared_ptr<HttpInterface> &http)
      : PackageManagerInterface(pconfig, bconfig, storage, http) {}
  ~PackageManagerFake() override = default;
  std::string name() const override { return "fake"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;

  data::InstallationResult install(const Uptane::Target &target) const override;
  void completeInstall() const override;
  data::InstallationResult finalizeInstall(const Uptane::Target &target) const override;
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) override;
};

#endif  // PACKAGEMANAGERFAKE_H_
