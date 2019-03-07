#ifndef ANDROIDMANAGER_H
#define ANDROIDMANAGER_H

#include "package_manager/packagemanagerinterface.h"

class AndroidInstallationDispatcher;

class AndroidManager : public PackageManagerInterface {
 public:
  explicit AndroidManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage,
                          std::shared_ptr<Bootloader> bootloader, std::shared_ptr<HttpInterface> http)
      : PackageManagerInterface(pconfig, storage, bootloader, http) {}
  ~AndroidManager() override = default;
  std::string name() const override { return "android"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;
  bool imageUpdated() override { return true; };

  data::InstallationResult install(const Uptane::Target& target) const override;
  data::InstallationResult finalizeInstall(const Uptane::Target& target) const override;

  static std::string GetOTAPackageFilePath(const std::string& hash);

 private:
  bool installationAborted(std::string* errorMessage) const;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<Bootloader> bootloader_;
  static const std::string data_ota_package_dir_;
};

#endif  // ANDROIDMANAGER_H
