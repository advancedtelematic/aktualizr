#ifndef EVENTS_H_
#define EVENTS_H_
/** \file */

#include <map>

#include <json/json.h>
#include <boost/signals2.hpp>

#include "uptane/tuf.h"
#include "utilities/channel.h"
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
using Channel = Channel<std::shared_ptr<BaseEvent>>;

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
 * The FetchMeta command completed successfully.
 */
class FetchMetaComplete : public BaseEvent {
 public:
  explicit FetchMetaComplete();
};

/**
 * The SendDeviceData command completed successfully.
 */
class SendDeviceDataComplete : public BaseEvent {
 public:
  explicit SendDeviceDataComplete();
};

/**
 * The PutManifest command completed successfully.
 * TODO: This actually signifies that we've informed the server of a successful
 * installation.
 */
class PutManifestComplete : public BaseEvent {
 public:
  explicit PutManifestComplete();
};

class UpdateAvailable : public BaseEvent {
 public:
  std::vector<Uptane::Target> updates;
  unsigned int ecus_count;
  explicit UpdateAvailable(std::vector<Uptane::Target> updates_in, unsigned int ecus_count_in);
  std::string toJson() override;
  static UpdateAvailable fromJson(const std::string& json_str);
};

// TODO: remove?
class UptaneTimestampUpdated : public BaseEvent {
 public:
  UptaneTimestampUpdated();
};

class DownloadProgressReport : public BaseEvent {
 public:
  Uptane::Target target;
  std::string description;
  unsigned int progress;
  std::string toJson() override;

  explicit DownloadProgressReport(Uptane::Target target_in, std::string description_in, unsigned int progress_in);
};

/**
 * The StartDownload command completed successfully.
 */
class DownloadComplete : public BaseEvent {
 public:
  std::vector<Uptane::Target> updates;
  std::string toJson() override;

  explicit DownloadComplete(std::vector<Uptane::Target> updates_in);
  static DownloadComplete fromJson(const std::string& json_str);
};

/**
 * Ecu stared instalation
 */
class InstallStarted : public BaseEvent {
 public:
  InstallStarted(Uptane::EcuSerial serial_in);
  Uptane::EcuSerial serial;
};

/**
 * Ecu finished installation
 */
class InstallComplete : public BaseEvent {
 public:
  InstallComplete(Uptane::EcuSerial serial_in);
  Uptane::EcuSerial serial;
};

class CampaignCheckComplete : public BaseEvent {
 public:
  explicit CampaignCheckComplete();
};

class CampaignAcceptComplete : public BaseEvent {
 public:
  explicit CampaignAcceptComplete();
};

}  // namespace event

using EventChannelPtr = std::shared_ptr<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>;

#endif
