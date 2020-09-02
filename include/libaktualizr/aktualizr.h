#ifndef AKTUALIZR_H_
#define AKTUALIZR_H_

#include <future>
#include <memory>

#include <boost/signals2.hpp>

#include "libaktualizr/config.h"
#include "libaktualizr/events.h"
#include "libaktualizr/secondaryinterface.h"

class SotaUptaneClient;
class INvStorage;

namespace api {
class CommandQueue;
}

/**
 * This class provides the main APIs necessary for launching and controlling
 * libaktualizr.
 */
class Aktualizr {
 public:
  /** Aktualizr requires a configuration object. Examples can be found in the
   *  config directory.
   *
   * @throw SQLException
   * @throw boost::filesystem::filesystem_error
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (filesystem failure; libsodium initialization failure)
   */
  explicit Aktualizr(const Config& config);

  Aktualizr(const Aktualizr&) = delete;
  Aktualizr& operator=(const Aktualizr&) = delete;
  ~Aktualizr();

  /**
   * Initialize aktualizr. Any Secondaries should be added before making this
   * call. This will provision with the server if required. This must be called
   * before using any other aktualizr functions except AddSecondary.
   *
   * @throw Initializer::Error and subclasses
   * @throw SQLException
   * @throw boost::filesystem::filesystem_error
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (curl, P11, filesystem, credentials archive
   *                            parsing, and certificate parsing failures;
   *                            missing ECU serials or device ID; database
   *                            inconsistency with pending updates; invalid
   *                            OSTree deployment)
   * @throw std::system_error (failure to lock a mutex)
   */
  void Initialize();

  /**
   * Asynchronously run aktualizr indefinitely until Shutdown is called.
   * @param custom_hwinfo if not empty will be sent to the backend instead of `lshw` output
   * @return Empty std::future object
   *
   * @throw SQLException
   * @throw boost::filesystem::filesystem_error
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (curl and filesystem failures; database
   *                            inconsistency with pending updates; error
   *                            getting metadata from database or filesystem)
   * @throw std::system_error (failure to lock a mutex)
   */
  std::future<void> RunForever(const Json::Value& custom_hwinfo = Json::nullValue);

  /**
   * Shuts down currently running `RunForever()` method
   *
   * @throw std::system_error (failure to lock a mutex)
   */
  void Shutdown();

  /**
   * Check for campaigns.
   * Campaigns are a concept outside of Uptane, and allow for user approval of
   * updates before the contents of the update are known.
   * @return std::future object with data about available campaigns.
   *
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (curl failure)
   */
  std::future<result::CampaignCheck> CampaignCheck();

  /**
   * Act on campaign: accept, decline or postpone.
   * Accepted campaign will be removed from the campaign list but no guarantee
   * is made for declined or postponed items. Applications are responsible for
   * tracking their state but this method will notify the server for device
   * state monitoring purposes.
   * @param campaign_id Campaign ID as provided by CampaignCheck.
   * @param cmd action to apply on the campaign: accept, decline or postpone
   * @return Empty std::future object
   *
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::system_error (failure to lock a mutex)
   */
  std::future<void> CampaignControl(const std::string& campaign_id, campaign::Cmd cmd);

  /**
   * Send local device data to the server.
   * This includes network status, installed packages, hardware etc.
   * @param custom_hwinfo if not empty will be sent to the backend instead of `lshw` output
   * @return Empty std::future object
   *
   * @throw SQLException
   * @throw boost::filesystem::filesystem_error
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (curl and filesystem failures)
   * @throw std::system_error (failure to lock a mutex)
   */
  std::future<void> SendDeviceData(const Json::Value& custom_hwinfo = Json::nullValue);

  /**
   * Fetch Uptane metadata and check for updates.
   * This collects a client manifest, PUTs it to the director, updates the
   * Uptane metadata (including root and targets), and then checks the metadata
   * for target updates.
   * @return Information about available updates.
   *
   * @throw SQLException
   * @throw boost::filesystem::filesystem_error
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (curl and filesystem failures; database
   *                            inconsistency with pending updates)
   * @throw std::system_error (failure to lock a mutex)
   */
  std::future<result::UpdateCheck> CheckUpdates();

  /**
   * Download targets.
   * @param updates Vector of targets to download as provided by CheckUpdates.
   * @return std::future object with information about download results.
   *
   * @throw SQLException
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::system_error (failure to lock a mutex)
   */
  std::future<result::Download> Download(const std::vector<Uptane::Target>& updates);

  struct InstallationLogEntry {
    Uptane::EcuSerial ecu;
    std::vector<Uptane::Target> installs;
  };
  using InstallationLog = std::vector<InstallationLogEntry>;

  /**
   * Get log of installations. The log is indexed for every ECU and contains
   * every change of versions ordered by time. It may contain duplicates in
   * case of rollbacks.
   * @return installation log
   *
   * @throw SQLException
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (failure to load ECU serials)
   */
  InstallationLog GetInstallationLog();

  /**
   * Get list of targets currently in storage. This is intended to be used with
   * DeleteStoredTarget and targets are not guaranteed to be verified and
   * up-to-date with current metadata.
   * @return std::vector of target objects
   *
   * @throw SQLException
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (error getting targets from database)
   */
  std::vector<Uptane::Target> GetStoredTargets();

  /**
   * Delete a stored target from storage. This only affects storage of the
   * actual binary data and does not preclude a re-download if a target matches
   * current metadata.
   * @param target Target object matching the desired target in the storage
   * @return true if successful
   *
   * @throw SQLException
   * @throw std::runtime_error (error getting targets from database or filesystem)
   */
  void DeleteStoredTarget(const Uptane::Target& target);

  /**
   * Get target downloaded in Download call. Returned target is guaranteed to be verified and up-to-date
   * according to the Uptane metadata downloaded in CheckUpdates call.
   * @param target Target object matching the desired target in the storage.
   * @return Handle to the stored binary. nullptr if none is found.
   *
   * @throw SQLException
   * @throw std::runtime_error (error getting targets from database or filesystem)
   */
  std::ifstream OpenStoredTarget(const Uptane::Target& target);

  /**
   * Install targets.
   * @param updates Vector of targets to install as provided by CheckUpdates or
   * Download.
   * @return std::future object with information about installation results.
   *
   * @throw SQLException
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (error getting metadata from database or filesystem)
   * @throw std::system_error (failure to lock a mutex)
   */
  std::future<result::Install> Install(const std::vector<Uptane::Target>& updates);

  /**
   * SetInstallationRawReport allows setting a custom raw report field in the device installation result.
   *
   * @note An invocation of this method will have effect only after call of Aktualizr::Install and before calling
   * Aktualizr::SendManifest member function.
   * @param custom_raw_report is intended to replace a default value in the device installation report.
   * @return true if the custom raw report was successfully applied to the device installation result.
   * If there is no installation report in the storage the function will always return false.
   *
   * @throw SQLException
   */
  bool SetInstallationRawReport(const std::string& custom_raw_report);

  /**
   * Send installation report to the backend.
   *
   * @note The device manifest is also sent as a part of CheckUpdates and
   * SendDeviceData calls, as well as after a reboot if it was initiated
   * by Aktualizr as a part of an installation process.
   * All these manifests will not include the custom data provided in this call.
   *
   * @param custom Project-specific data to put in the custom field of Uptane manifest
   * @return std::future object with manifest update result (true on success).
   *
   * @throw SQLException
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (curl failure; database inconsistency with pending updates)
   * @throw std::system_error (failure to lock a mutex)
   */
  std::future<bool> SendManifest(const Json::Value& custom = Json::nullValue);

  /**
   * Pause the library operations.
   * In progress target downloads will be paused and API calls will be deferred.
   *
   * @return Information about pause results.
   *
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::system_error (failure to lock a mutex)
   */
  result::Pause Pause();

  /**
   * Resume the library operations.
   * Target downloads will resume and API calls issued during the pause will
   * execute in fifo order.
   *
   * @return Information about resume results.
   *
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::system_error (failure to lock a mutex)
   */
  result::Pause Resume();

  /**
   * Aborts the currently running command, if it can be aborted, or waits for it
   * to finish; then removes all other queued calls.
   * This doesn't reset the `Paused` state, i.e. if the queue was previously
   * paused, it will remain paused, but with an emptied queue.
   * The call is blocking.
   *
   * @throw std::system_error (failure to lock a mutex)
   */
  void Abort();

  /**
   * Synchronously run an Uptane cycle: check for updates, download any new
   * targets, install them, and send a manifest back to the server.
   *
   * @return `false`, if the restart is required to continue, `true` otherwise
   *
   * @throw SQLException
   * @throw boost::filesystem::filesystem_error
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (curl and filesystem failures; database
   *                            inconsistency with pending updates; error
   *                            getting metadata from database or filesystem)
   * @throw std::system_error (failure to lock a mutex)
   */
  bool UptaneCycle();

  /**
   * Add new Secondary to aktualizr. Must be called before Initialize.
   * @param secondary An object to perform installation on a Secondary ECU.
   *
   * @throw std::bad_alloc (memory allocation failure)
   * @throw std::runtime_error (multiple Secondaries with the same serial)
   */
  void AddSecondary(const std::shared_ptr<SecondaryInterface>& secondary);

  /**
   * Store some free-form data to be associated with a particular Secondary, to
   * be retrieved later through `GetSecondaries`
   *
   * @throw SQLException
   */
  void SetSecondaryData(const Uptane::EcuSerial& ecu, const std::string& data);

  /**
   * Returns a list of the registered Secondaries, along with some associated
   * metadata
   *
   * @return vector of SecondaryInfo objects
   *
   * @throw SQLException
   * @throw std::bad_alloc (memory allocation failure)
   */
  std::vector<SecondaryInfo> GetSecondaries() const;

  // The type proxy is needed in doxygen 1.8.16 because of this bug
  // https://github.com/doxygen/doxygen/issues/7236
  using SigHandler = std::function<void(std::shared_ptr<event::BaseEvent>)>;

  /**
   * Provide a function to receive event notifications.
   * @param handler a function that can receive event objects.
   * @return a signal connection object, which can be disconnected if desired.
   */
  boost::signals2::connection SetSignalHandler(const SigHandler& handler);

 private:
  // Make sure this is declared before SotaUptaneClient to prevent Valgrind
  // complaints with destructors.
  Config config_;

 protected:
  Aktualizr(Config config, std::shared_ptr<INvStorage> storage_in, const std::shared_ptr<HttpInterface>& http_in);

  std::shared_ptr<SotaUptaneClient> uptane_client_;

 private:
  struct {
    std::mutex m;
    std::condition_variable cv;
    bool flag = false;
  } exit_cond_;

  std::shared_ptr<INvStorage> storage_;
  std::shared_ptr<event::Channel> sig_;
  std::unique_ptr<api::CommandQueue> api_queue_;
};

#endif  // AKTUALIZR_H_
