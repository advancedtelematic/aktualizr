#ifndef EVENTS_H_
#define EVENTS_H_
/** \file */

#include <map>

#include <json/json.h>
#include <boost/signals2.hpp>

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
class DownloadComplete : public BaseEvent {
 public:
  std::vector<Uptane::Target> updates;
  std::string toJson() override;

  explicit DownloadComplete(std::vector<Uptane::Target> updates_in);
  static DownloadComplete fromJson(const std::string& json_str);
};

/**
 * An ECU has begun installation of an update.
 */
class InstallStarted : public BaseEvent {
 public:
  InstallStarted(Uptane::EcuSerial serial_in);
  Uptane::EcuSerial serial;
};

/**
 * An update has been successfully installed on an ECU.
 */
class InstallComplete : public BaseEvent {
 public:
  InstallComplete(Uptane::EcuSerial serial_in);
  Uptane::EcuSerial serial;
};

/**
 * The server has been successfully queried for available campaigns.
 */
class CampaignCheckComplete : public BaseEvent {
 public:
  explicit CampaignCheckComplete();
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
