#ifndef ANDROIDMANAGER_H
#define ANDROIDMANAGER_H

#include <memory>

#include "libaktualizr/packagemanagerinterface.h"

class AndroidInstallationDispatcher;

class AndroidManager : public PackageManagerInterface {
 public:
  explicit AndroidManager(const PackageConfig& pconfig, const BootloaderConfig& bconfig,
                          const std::shared_ptr<INvStorage>& storage, const std::shared_ptr<Bootloader>& bootloader,
                          const std::shared_ptr<HttpInterface>& http)
      : PackageManagerInterface(pconfig, bconfig, storage, http) {}
  ~AndroidManager() override = default;
  std::string name() const override { return "android"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;

  data::InstallationResult install(const Uptane::Target& target) const override;
  data::InstallationResult finalizeInstall(const Uptane::Target& target) override;

  static std::string GetOTAPackageFilePath(const std::string& hash);

 private:
  bool installationAborted(std::string* errorMessage) const;
  static const std::string data_ota_package_dir_;
};

#endif  // ANDROIDMANAGER_H
