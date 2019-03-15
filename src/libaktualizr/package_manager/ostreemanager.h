#ifndef OSTREE_H_
#define OSTREE_H_

#include <memory>
#include <string>

#include <glib/gi18n.h>
#include <ostree.h>

#include "bootloader/bootloader.h"
#include "crypto/keymanager.h"
#include "packagemanagerconfig.h"
#include "packagemanagerinterface.h"
#include "utilities/types.h"

const char remote[] = "aktualizr-remote";

template <typename T>
struct GObjectFinalizer {
  void operator()(T *e) const { g_object_unref(reinterpret_cast<gpointer>(e)); }
};

template <typename T>
using GObjectUniquePtr = std::unique_ptr<T, GObjectFinalizer<T>>;
using OstreeProgressCb = std::function<void(const Uptane::Target &, const std::string &, unsigned int)>;

struct PullMetaStruct {
  PullMetaStruct(Uptane::Target target_in, const std::function<void()> &pause_cb, GCancellable *cancellable_in,
                 OstreeProgressCb progress_cb_in)
      : target{std::move(target_in)},
        percent_complete{0},
        check_pause{pause_cb},
        cancellable{cancellable_in},
        progress_cb{std::move(progress_cb_in)} {}
  Uptane::Target target;
  unsigned int percent_complete;
  const std::function<void()> &check_pause;
  GObjectUniquePtr<GCancellable> cancellable;
  OstreeProgressCb progress_cb;
};

class OstreeManager : public PackageManagerInterface {
 public:
  OstreeManager(PackageConfig pconfig, std::shared_ptr<INvStorage> storage, std::shared_ptr<Bootloader> bootloader);
  ~OstreeManager() override = default;
  std::string name() const override { return "ostree"; }
  Json::Value getInstalledPackages() const override;
  Uptane::Target getCurrent() const override;
  bool imageUpdated() override;
  data::InstallationResult install(const Uptane::Target &target) const override;
  void completeInstall() const override;
  data::InstallationResult finalizeInstall(const Uptane::Target &target) const override;

  GObjectUniquePtr<OstreeDeployment> getStagedDeployment() const;
  static GObjectUniquePtr<OstreeSysroot> LoadSysroot(const boost::filesystem::path &path);
  static GObjectUniquePtr<OstreeRepo> LoadRepo(OstreeSysroot *sysroot, GError **error);
  static bool addRemote(OstreeRepo *repo, const std::string &url, const KeyManager &keys);
  static data::InstallationResult pull(const boost::filesystem::path &sysroot_path, const std::string &ostree_server,
                                       const KeyManager &keys, const Uptane::Target &target,
                                       const std::function<void()> &pause_cb = {},
                                       OstreeProgressCb progress_cb = nullptr);

 private:
  PackageConfig config;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<Bootloader> bootloader_;
};

#endif  // OSTREE_H_
