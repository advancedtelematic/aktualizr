#ifndef DEB_H_
#define DEB_H_

#include <mutex>
#include <string>
#include <utility>

#include "packagemanagerinterface.h"

class DebianManager : public PackageManagerInterface {
 public:
  DebianManager(const PackageConfig& pconfig, const BootloaderConfig& bconfig,
                const std::shared_ptr<INvStorage>& storage, const std::shared_ptr<HttpInterface>& http)
      : PackageManagerInterface(pconfig, bconfig, storage, http) {}
  ~DebianManager() override = default;
  std::string name() const override { return "debian"; }
  Json::Value getInstalledPackages() const override;
  Uptane::Target getCurrent() const override;
  data::InstallationResult install(const Uptane::Target& target) const override;
  data::InstallationResult finalizeInstall(const Uptane::Target& target) override {
    (void)target;
    throw std::runtime_error("Unimplemented");
  }

 private:
  mutable std::mutex mutex_;
};

#endif  // DEB_H_
