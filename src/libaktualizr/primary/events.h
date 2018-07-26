#ifndef EVENTS_H_
#define EVENTS_H_
/** \file */

#include <map>

#include <json/json.h>

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
  virtual ~BaseEvent() = default;
  std::string variant;
  virtual std::string toJson(Json::Value json);
  virtual std::string toJson();
};
using Channel = Channel<std::shared_ptr<BaseEvent> >;

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
 */
class PutManifestComplete : public BaseEvent {
 public:
  explicit PutManifestComplete();
};

class UpdateAvailable : public BaseEvent {
 public:
  std::vector<Uptane::Target> updates;
  explicit UpdateAvailable(std::vector<Uptane::Target> updates_in);
  std::string toJson() override;
  static UpdateAvailable fromJson(const std::string& json_str);
};

class UptaneTimestampUpdated : public BaseEvent {
 public:
  UptaneTimestampUpdated();
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
 * The UptaneInstall command completed successfully.
 */
class InstallComplete : public BaseEvent {
 public:
  explicit InstallComplete();
};

}  // namespace event

#endif
