#ifndef EVENTS_H_
#define EVENTS_H_
/** \file */

#include <map>

#include <json/json.h>
#include <boost/signals2.hpp>

#include "campaign/campaign.h"
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
  virtual std::string toJson(Json::Value json);
  virtual std::string toJson();
};

/**
 * An error occurred processing a command.
 */
class Error : public BaseEvent {
 public:
  std::string message;
  explicit Error(std::string /*message_in*/);
  std::string toJson() override;
  static Error fromJson(const std::string& /*json_str*/);
};

/**
 * Metadata has been successfully fetched from the server.
 */
class FetchMetaComplete : public BaseEvent {
 public:
  explicit FetchMetaComplete();
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
 * No update is available for download from the server.
 */
class NoUpdateAvailable : public BaseEvent {
 public:
  explicit NoUpdateAvailable();
};

/**
 * An update is available for download from the server.
 */
class UpdateAvailable : public BaseEvent {
 public:
  std::vector<Uptane::Target> updates;
  unsigned int ecus_count;
  explicit UpdateAvailable(std::vector<Uptane::Target> updates_in, unsigned int ecus_count_in);
  std::string toJson() override;
  static UpdateAvailable fromJson(const std::string& json_str);
};

/**
 * Nothing to download from the server.
 */
class NothingToDownload : public BaseEvent {
 public:
  explicit NothingToDownload();
};

/**
 * A report for a download in progress.
 */
class DownloadProgressReport : public BaseEvent {
 public:
  Uptane::Target target;
  std::string description;
  unsigned int progress;
  std::string toJson() override;

  explicit DownloadProgressReport(Uptane::Target target_in, std::string description_in, unsigned int progress_in);
};

/**
 * An update has been successfully downloaded.
 */
class AllDownloadsComplete : public BaseEvent {
 public:
  std::vector<Uptane::Target> updates;
  std::string toJson() override;

  explicit AllDownloadsComplete(std::vector<Uptane::Target> updates_in);
  static AllDownloadsComplete fromJson(const std::string& json_str);
};

/**
 * A target has been successfully downloaded.
 */
class DownloadTargetComplete : public BaseEvent {
 public:
  Uptane::Target update;
  std::string toJson() override;

  explicit DownloadTargetComplete(Uptane::Target update_in);
  static DownloadTargetComplete fromJson(const std::string& json_str);
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
  explicit CampaignCheckComplete(std::vector<campaign::Campaign> campaigns_in);
  std::vector<campaign::Campaign> campaigns;
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

#endif
