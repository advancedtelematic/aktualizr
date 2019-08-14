#ifndef DOCKERAPPMGR_H_
#define DOCKERAPPMGR_H_

#include "ostreemanager.h"
#include "uptane/iterator.h"

using DockerAppCb = std::function<bool(const std::string &app, const Uptane::Target &app_target)>;

class DockerAppManager : public OstreeManager {
 public:
  DockerAppManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage, std::shared_ptr<Bootloader> bootloader,
                   std::shared_ptr<HttpInterface> http)
      : OstreeManager(std::move(pconfig), std::move(storage), std::move(bootloader), std::move(http)) {
    fake_fetcher_ = std::make_shared<Uptane::Fetcher>(Config(), http_);
  }
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) override;
  data::InstallationResult install(const Uptane::Target &target) const override;
  std::string name() const override { return "ostree+docker-app"; }

 private:
  bool iterate_apps(const Uptane::Target &target, const DockerAppCb &cb) const;

  // iterate_apps needs an Uptane::Fetcher. However, its an unused parameter
  // and we just need to construct a dummy one to make the compiler happy.
  std::shared_ptr<Uptane::Fetcher> fake_fetcher_;
};
#endif  // DOCKERAPPMGR_H_
