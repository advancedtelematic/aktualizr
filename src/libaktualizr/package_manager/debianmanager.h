#ifndef DEB_H_
#define DEB_H_

#include <mutex>
#include <string>
#include <utility>

#include "packagemanagerinterface.h"
#include "utilities/types.h"


class DebianManager : public PackageManagerBase {
 public:
  DebianManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage, std::shared_ptr<Bootloader> bootloader)
      : PackageManagerBase(pconfig, storage, bootloader) {}
  ~DebianManager() override = default;
  std::string name() const override { return "debian"; }
  Json::Value getInstalledPackages() const override;
  Uptane::Target getCurrent() const override;
  data::InstallationResult install(const Uptane::Target &target) const override;
  data::InstallationResult finalizeInstall(const Uptane::Target &target) const override {
    (void)target;
    throw std::runtime_error("Unimplemented");
  }
  bool imageUpdated() override { return true; }

 private:
  mutable std::mutex mutex_;
};

#endif  // DEB_H_
