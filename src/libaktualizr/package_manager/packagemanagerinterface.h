#ifndef PACKAGEMANAGERINTERFACE_H_
#define PACKAGEMANAGERINTERFACE_H_

#include <mutex>
#include <string>

#include "bootloader/bootloader.h"
#include "crypto/keymanager.h"
#include "http/httpinterface.h"
#include "packagemanagerconfig.h"
#include "storage/invstorage.h"
#include "uptane/fetcher.h"
#include "utilities/apiqueue.h"
#include "utilities/types.h"

using FetcherProgressCb = std::function<void(const Uptane::Target&, const std::string&, unsigned int)>;

/**
 * Status of downloaded target.
 */
enum class TargetStatus {
  /* Target has been downloaded and verified. */
  kGood = 0,
  /* Target was not found. */
  kNotFound,
  /* Target was found, but is incomplete. */
  kIncomplete,
  /* Target was found, but is larger than expected. */
  kOversized,
  /* Target was found, but hash did not match the metadata. */
  kHashMismatch,
  /* Target was found and has valid metadata but the content is not suitable for the packagemanager */
  kInvalid,
};

class PackageManagerInterface {
 public:
  PackageManagerInterface(PackageConfig pconfig, BootloaderConfig bconfig, std::shared_ptr<INvStorage> storage,
                          std::shared_ptr<HttpInterface> http)
      : config(std::move(pconfig)), storage_(std::move(storage)), http_(std::move(http)) {
    (void)bconfig;
  }
  virtual ~PackageManagerInterface() = default;
  virtual std::string name() const = 0;
  virtual Json::Value getInstalledPackages() const = 0;
  virtual Uptane::Target getCurrent() const = 0;
  virtual data::InstallationResult install(const Uptane::Target& target) const = 0;
  virtual void completeInstall() const { throw std::runtime_error("Unimplemented"); };
  virtual data::InstallationResult finalizeInstall(const Uptane::Target& target) = 0;
  virtual void updateNotify(){};
  virtual bool fetchTarget(const Uptane::Target& target, Uptane::Fetcher& fetcher, const KeyManager& keys,
                           FetcherProgressCb progress_cb, const api::FlowControlToken* token);
  virtual TargetStatus verifyTarget(const Uptane::Target& target) const;

 protected:
  PackageConfig config;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<HttpInterface> http_;
};
#endif  // PACKAGEMANAGERINTERFACE_H_
