#ifndef EVENTS_H_
#define EVENTS_H_
/** \file */

#include <memory>
#include <string>

#include <boost/signals2.hpp>

#include "primary/results.h"
#include "uptane/fetcher.h"
#include "uptane/tuf.h"
#include "utilities/types.h"

/**
 * Aktualizr status events.
 */
namespace event {

/**
 * Base class for all event objects.
 */
class BaseEvent {
 public:
  BaseEvent() = default;
  BaseEvent(std::string variant_in) : variant(std::move(variant_in)) {}
  virtual ~BaseEvent() = default;
  std::string variant;
};

/**
 * Device data has been sent to the server.
 */
class SendDeviceDataComplete : public BaseEvent {
 public:
  SendDeviceDataComplete() { variant = "SendDeviceDataComplete"; }
};

/**
 * A manifest has been sent to the server.
 */
class PutManifestComplete : public BaseEvent {
 public:
  explicit PutManifestComplete(bool success_in) : success(success_in) { variant = "PutManifestComplete"; }
  bool success;
};

/**
 * An update is available for download from the server.
 */
class UpdateCheckComplete : public BaseEvent {
 public:
  explicit UpdateCheckComplete(UpdateCheckResult result_in) : result(std::move(result_in)) {
    variant = "UpdateCheckComplete";
  }

  UpdateCheckResult result;
};

/**
 * A download in progress has been paused.
 */
class DownloadPaused : public BaseEvent {
 public:
  explicit DownloadPaused(PauseResult result_in) : result(result_in) { variant = "DownloadPaused"; }
  PauseResult result;
};

/**
 * A paused download has been resumed.
 */
class DownloadResumed : public BaseEvent {
 public:
  explicit DownloadResumed(PauseResult result_in) : result(result_in) { variant = "DownloadResumed"; }
  PauseResult result;
};

/**
 * A report for a download in progress.
 */
class DownloadProgressReport : public BaseEvent {
 public:
  DownloadProgressReport(Uptane::Target target_in, std::string description_in, unsigned int progress_in)
      : target{std::move(target_in)}, description{std::move(description_in)}, progress{progress_in} {
    variant = "DownloadProgressReport";
  }

  Uptane::Target target;
  std::string description;
  unsigned int progress;
};

/**
 * A target has been downloaded.
 */
class DownloadTargetComplete : public BaseEvent {
 public:
  DownloadTargetComplete(Uptane::Target update_in, bool success_in)
      : update(std::move(update_in)), success(success_in) {
    variant = "DownloadTargetComplete";
  }

  Uptane::Target update;
  bool success;
};

/**
 * All targets for an update have been downloaded.
 */
class AllDownloadsComplete : public BaseEvent {
 public:
  explicit AllDownloadsComplete(DownloadResult result_in) : result(std::move(result_in)) {
    variant = "AllDownloadsComplete";
  }

  DownloadResult result;
};

/**
 * An ECU has begun installation of an update.
 */
class InstallStarted : public BaseEvent {
 public:
  explicit InstallStarted(Uptane::EcuSerial serial_in) : serial(std::move(serial_in)) { variant = "InstallStarted"; }
  Uptane::EcuSerial serial;
};

/**
 * An installation attempt on an ECU has completed.
 */
class InstallTargetComplete : public BaseEvent {
 public:
  InstallTargetComplete(Uptane::EcuSerial serial_in, bool success_in)
      : serial(std::move(serial_in)), success(success_in) {
    variant = "InstallTargetComplete";
  }

  Uptane::EcuSerial serial;
  bool success;
};

/**
 * All ECU installation attempts for an update have completed.
 */
class AllInstallsComplete : public BaseEvent {
 public:
  explicit AllInstallsComplete(InstallResult result_in) : result(std::move(result_in)) {
    variant = "AllInstallsComplete";
  }

  InstallResult result;
};

/**
 * The server has been queried for available campaigns.
 */
class CampaignCheckComplete : public BaseEvent {
 public:
  explicit CampaignCheckComplete(CampaignCheckResult result_in) : result(std::move(result_in)) {
    variant = "CampaignCheckComplete";
  }

  CampaignCheckResult result;
};

/**
 * A campaign has been accepted.
 */
class CampaignAcceptComplete : public BaseEvent {
 public:
  CampaignAcceptComplete() { variant = "CampaignAcceptComplete"; }
};

using Channel = boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>;

}  // namespace event

#endif  // EVENTS_H_
