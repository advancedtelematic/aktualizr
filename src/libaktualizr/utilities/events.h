#ifndef EVENTS_H_
#define EVENTS_H_
/** \file */

#include <map>
#include <string>

#include <json/json.h>
#include <boost/signals2.hpp>

#include "campaign/campaign.h"
#include "uptane/tuf.h"
#include "utilities/results.h"
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
 * An error occurred processing a command.
 */
class Error : public BaseEvent {
 public:
  explicit Error(std::string /*message_in*/);
  std::string message;
};

/**
 * Device data has been successfully sent to the server.
 */
class SendDeviceDataComplete : public BaseEvent {
 public:
  explicit SendDeviceDataComplete();
};

/**
 * A manifest has been successfully sent to the server.
 */
class PutManifestComplete : public BaseEvent {
 public:
  explicit PutManifestComplete();
};

/**
 * An update is available for download from the server.
 */
class UpdateCheckComplete : public BaseEvent {
 public:
  explicit UpdateCheckComplete(UpdateCheckResult result_in);
  UpdateCheckResult result;
};

/**
 * A report for a download in progress.
 */
class DownloadProgressReport : public BaseEvent {
 public:
  DownloadProgressReport(Uptane::Target target_in, std::string description_in, unsigned int progress_in);
  Uptane::Target target;
  std::string description;
  unsigned int progress;
};

/**
 * A target has been successfully downloaded.
 */
class DownloadTargetComplete : public BaseEvent {
 public:
  explicit DownloadTargetComplete(Uptane::Target update_in);
  Uptane::Target update;
};

/**
 * An update has been successfully downloaded.
 */
class AllDownloadsComplete : public BaseEvent {
 public:
  explicit AllDownloadsComplete(DownloadResult result_in);
  DownloadResult result;
};

/**
 * An ECU has begun installation of an update.
 */
class InstallStarted : public BaseEvent {
 public:
  explicit InstallStarted(Uptane::EcuSerial serial_in);
  Uptane::EcuSerial serial;
};

/**
 * An update attempt on an ECU is finished.
 */
class InstallTargetComplete : public BaseEvent {
 public:
  InstallTargetComplete(Uptane::EcuSerial serial_in, bool success_in);
  Uptane::EcuSerial serial;
  bool success;
};

/**
 * All ECU update attempts have completed.
 */
class AllInstallsComplete : public BaseEvent {
 public:
  AllInstallsComplete();
};

/**
 * The server has been successfully queried for available campaigns.
 */
class CampaignCheckComplete : public BaseEvent {
 public:
  explicit CampaignCheckComplete(CampaignCheckResult result_in);
  CampaignCheckResult result;
};

/**
 * A campaign has been successfully accepted.
 */
class CampaignAcceptComplete : public BaseEvent {
 public:
  explicit CampaignAcceptComplete();
};

using Channel = boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>;

}  // namespace event

#endif  // EVENTS_H_
