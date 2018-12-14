#ifndef ANDROIDMANAGER_H
#define ANDROIDMANAGER_H

#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"

class AndroidInstallationDispatcher;

class AndroidManager : public PackageManagerInterface {
 public:
  explicit AndroidManager(std::shared_ptr<INvStorage> storage) : storage_(std::move(storage)) {}
  ~AndroidManager() override = default;
  std::string name() const override { return "android"; }
  Json::Value getInstalledPackages() const override;

  Uptane::Target getCurrent() const override;
  bool imageUpdated() override { return true; };

  data::InstallOutcome install(const Uptane::Target &target) const override;
  data::InstallOutcome finalizeInstall(const Uptane::Target &target) const override;

 private:
  std::shared_ptr<INvStorage> storage_;
};

struct AndroidInstallationState {
  enum State { STATE_NOP, STATE_READY, STATE_IN_PROGRESS, STATE_INSTALLED, STATE_UPDATE };

  State state_;
  std::string payload_;
};

class AndroidInstallationDispatcher {
 public:
  static AndroidInstallationState GetState();
  static AndroidInstallationState Dispatch();
};

#endif  // ANDROIDMANAGER_H
