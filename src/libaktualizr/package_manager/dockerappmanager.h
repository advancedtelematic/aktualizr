#ifndef DOCKERAPPMGR_H_
#define DOCKERAPPMGR_H_

#include "ostreemanager.h"
#include "uptane/iterator.h"

using DockerAppCb = std::function<bool(const std::string &app, const Uptane::Target &app_target)>;

class DockerAppManagerConfig {
 public:
  DockerAppManagerConfig(const PackageConfig &pconfig);

  std::vector<std::string> docker_apps;
  boost::filesystem::path docker_apps_root;
  boost::filesystem::path docker_app_params;
  boost::filesystem::path docker_app_bin{"/usr/bin/docker-app"};
  boost::filesystem::path docker_compose_bin{"/usr/bin/docker-compose"};
};

class DockerAppStandalone : public OstreeManager {
 public:
  DockerAppStandalone(const PackageConfig &pconfig, const BootloaderConfig &bconfig,
                      const std::shared_ptr<INvStorage> &storage, const std::shared_ptr<HttpInterface> &http)
      : OstreeManager(pconfig, bconfig, storage, http), dappcfg_(pconfig) {
    fake_fetcher_ = std::make_shared<Uptane::Fetcher>("", "", http_);
  }
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) override;
  data::InstallationResult install(const Uptane::Target &target) const override;
  TargetStatus verifyTarget(const Uptane::Target &target) const override;
  std::string name() const override { return "ostree+docker-app-standalone"; }

 private:
  DockerAppManagerConfig dappcfg_;

  bool iterate_apps(const Uptane::Target &target, const DockerAppCb &cb) const;
  void handleRemovedApps(const Uptane::Target &target) const;

  // iterate_apps needs an Uptane::Fetcher. However, its an unused parameter
  // and we just need to construct a dummy one to make the compiler happy.
  std::shared_ptr<Uptane::Fetcher> fake_fetcher_;
};

class DockerAppBundles : public OstreeManager {
 public:
  DockerAppBundles(const PackageConfig &pconfig, const BootloaderConfig &bconfig,
                   const std::shared_ptr<INvStorage> &storage, const std::shared_ptr<HttpInterface> &http)
      : OstreeManager(pconfig, bconfig, storage, http), dappcfg_(pconfig) {
    fake_fetcher_ = std::make_shared<Uptane::Fetcher>("", "", http_);
  }
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) override;
  data::InstallationResult install(const Uptane::Target &target) const override;
  std::string name() const override { return "ostree+docker-app-bundles"; }

 private:
  DockerAppManagerConfig dappcfg_;

  std::vector<std::pair<std::string, std::string>> iterate_apps(const Uptane::Target &target) const;
  void handleRemovedApps(const Uptane::Target &target) const;

  // iterate_apps needs an Uptane::Fetcher. However, its an unused parameter
  // and we just need to construct a dummy one to make the compiler happy.
  std::shared_ptr<Uptane::Fetcher> fake_fetcher_;
};

class DockerAppManager : public OstreeManager {
 public:
  DockerAppManager(const PackageConfig &pconfig, const BootloaderConfig &bconfig,
                   const std::shared_ptr<INvStorage> &storage, const std::shared_ptr<HttpInterface> &http)
      : OstreeManager(pconfig, bconfig, storage, http) {
    auto standalone = DockerAppManagerConfig(pconfig).docker_app_bin;
    if (boost::filesystem::exists(standalone)) {
      LOG_INFO << "DockerAppManager detects docker-app standalone environment";
      impl_ = std_::make_unique<DockerAppStandalone>(pconfig, bconfig, storage, http);
    } else {
      LOG_INFO << "DockerAppManager detects docker app bundle environment";
      impl_ = std_::make_unique<DockerAppBundles>(pconfig, bconfig, storage, http);
    }
  }
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) override {
    return impl_->fetchTarget(target, fetcher, keys, progress_cb, token);
  }
  data::InstallationResult install(const Uptane::Target &target) const override { return impl_->install(target); }
  TargetStatus verifyTarget(const Uptane::Target &target) const override { return impl_->verifyTarget(target); }
  std::string name() const override { return "ostree+docker-app"; }

 private:
  std::unique_ptr<OstreeManager> impl_;
};

#endif  // DOCKERAPPMGR_H_
