#ifndef EVENTS_H_
#define EVENTS_H_

#include <map>

#include <json/json.h>

#include "uptane/tuf.h"
#include "utilities/channel.h"
#include "utilities/types.h"

namespace event {

class BaseEvent {
 public:
  virtual ~BaseEvent() = default;
  std::string variant;
  virtual std::string toJson(Json::Value json);
  virtual std::string toJson();
};
using Channel = Channel<std::shared_ptr<BaseEvent> >;

class Error : public BaseEvent {
 public:
  std::string message;
  explicit Error(std::string /*message_in*/);
  std::string toJson() override;
  static Error fromJson(const std::string& /*json_str*/);
};

class FetchMetaComplete : public BaseEvent {
 public:
  explicit FetchMetaComplete();
};

class SendDeviceDataComplete : public BaseEvent {
 public:
  explicit SendDeviceDataComplete();
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

class DownloadComplete : public BaseEvent {
 public:
  std::vector<Uptane::Target> updates;
  std::string toJson() override;

  explicit DownloadComplete(std::vector<Uptane::Target> updates_in);
  static DownloadComplete fromJson(const std::string& json_str);
};

class InstallComplete : public BaseEvent {
 public:
  explicit InstallComplete();
};

}  // namespace event

#endif
