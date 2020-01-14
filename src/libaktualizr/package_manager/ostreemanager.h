#ifndef OSTREE_H_
#define OSTREE_H_

#include <memory>
#include <string>

#include <glib/gi18n.h>
#include <ostree.h>

#include "crypto/keymanager.h"
#include "packagemanagerinterface.h"
#include "utilities/apiqueue.h"

const char remote[] = "aktualizr-remote";

template <typename T>
struct GObjectFinalizer {
  void operator()(T *e) const { g_object_unref(reinterpret_cast<gpointer>(e)); }
};

template <typename T>
using GObjectUniquePtr = std::unique_ptr<T, GObjectFinalizer<T>>;
using OstreeProgressCb = std::function<void(const Uptane::Target &, const std::string &, unsigned int)>;

struct PullMetaStruct {
  PullMetaStruct(Uptane::Target target_in, const api::FlowControlToken *token_in, GCancellable *cancellable_in,
                 OstreeProgressCb progress_cb_in)
      : target{std::move(target_in)},
        percent_complete{0},
        token{token_in},
        cancellable{cancellable_in},
        progress_cb{std::move(progress_cb_in)} {}
  Uptane::Target target;
  unsigned int percent_complete;
  const api::FlowControlToken *token;
  GObjectUniquePtr<GCancellable> cancellable;
  OstreeProgressCb progress_cb;
};

class OstreeManager : public PackageManagerInterface {
 public:
  OstreeManager(const PackageConfig &pconfig, const BootloaderConfig &bconfig,
                const std::shared_ptr<INvStorage> &storage, const std::shared_ptr<HttpInterface> &http)
      : OstreeManager(pconfig, std::make_shared<Bootloader>(bconfig, storage), storage, http) {}

  OstreeManager(PackageConfig pconfig, std::shared_ptr<Bootloader> bootloader, std::shared_ptr<INvStorage> storage,
                std::shared_ptr<HttpInterface> http);
  ~OstreeManager() override = default;
  std::string name() const override { return "ostree"; }
  Json::Value getInstalledPackages() const override;
  std::string getCurrentHash() const;
  Uptane::Target getCurrent() const override;
  bool imageUpdated();
  data::InstallationResult install(const Uptane::Target &target) const override;
  void completeInstall() const override;
  data::InstallationResult finalizeInstall(const Uptane::Target &target) const override;
  bool fetchTarget(const Uptane::Target &target, Uptane::Fetcher &fetcher, const KeyManager &keys,
                   FetcherProgressCb progress_cb, const api::FlowControlToken *token) override;
  TargetStatus verifyTarget(const Uptane::Target &target) const override;

  GObjectUniquePtr<OstreeDeployment> getStagedDeployment() const;
  static GObjectUniquePtr<OstreeSysroot> LoadSysroot(const boost::filesystem::path &path);
  static GObjectUniquePtr<OstreeRepo> LoadRepo(OstreeSysroot *sysroot, GError **error);
  static bool addRemote(OstreeRepo *repo, const std::string &url, const KeyManager &keys);
  static data::InstallationResult pull(const boost::filesystem::path &sysroot_path, const std::string &ostree_server,
                                       const KeyManager &keys, const Uptane::Target &target,
                                       const api::FlowControlToken *token = nullptr,
                                       OstreeProgressCb progress_cb = nullptr);

 private:
  TargetStatus verifyTargetInternal(const Uptane::Target &target) const;
};

#endif  // OSTREE_H_
