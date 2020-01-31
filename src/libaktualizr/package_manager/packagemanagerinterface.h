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
      : config(std::move(pconfig)),
        storage_(std::move(storage)),
        http_(std::move(http)),
        bootloader_{new Bootloader(std::move(bconfig), *storage_)} {}
  virtual ~PackageManagerInterface() = default;
  virtual std::string name() const = 0;
  virtual Json::Value getInstalledPackages() const = 0;
  virtual Uptane::Target getCurrent() const = 0;
  virtual data::InstallationResult install(const Uptane::Target& target) const = 0;
  virtual void completeInstall() const { throw std::runtime_error("Unimplemented"); };
  virtual data::InstallationResult finalizeInstall(const Uptane::Target& target) = 0;
  virtual bool rebootDetected() { return bootloader_->rebootDetected(); };
  virtual void rebootFlagClear() { bootloader_->rebootFlagClear(); };
  virtual void updateNotify() { bootloader_->updateNotify(); };
  virtual bool fetchTarget(const Uptane::Target& target, Uptane::Fetcher& fetcher, const KeyManager& keys,
                           FetcherProgressCb progress_cb, const api::FlowControlToken* token);
  virtual TargetStatus verifyTarget(const Uptane::Target& target) const;

  // only returns the version
  Json::Value getManifest(const Uptane::EcuSerial& ecu_serial) const {
    Uptane::Target installed_target = getCurrent();
    Json::Value installed_image;
    installed_image["filepath"] = installed_target.filename();
    installed_image["fileinfo"]["length"] = Json::UInt64(installed_target.length());
    installed_image["fileinfo"]["hashes"]["sha256"] = installed_target.sha256Hash();

    Json::Value unsigned_ecu_version;
    unsigned_ecu_version["attacks_detected"] = "";
    unsigned_ecu_version["installed_image"] = installed_image;
    unsigned_ecu_version["ecu_serial"] = ecu_serial.ToString();
    unsigned_ecu_version["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
    unsigned_ecu_version["timeserver_time"] = "1970-01-01T00:00:00Z";
    return unsigned_ecu_version;
  }

 protected:
  PackageConfig config;
  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<HttpInterface> http_;
  std::unique_ptr<Bootloader> bootloader_;
};
#endif  // PACKAGEMANAGERINTERFACE_H_
