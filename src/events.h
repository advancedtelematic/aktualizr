#ifndef EVENTS_H_
#define EVENTS_H_

#include <map>

#include <json/json.h>

#include "channel.h"
#include "uptane/tuf.h"
#include "utilities/types.h"

namespace event {

class BaseEvent {
 public:
  virtual ~BaseEvent() = default;
  std::string variant;
  Json::Value toBaseJson();
  virtual std::string toJson() = 0;
};
using Channel = Channel<std::shared_ptr<BaseEvent> >;

class Error : public BaseEvent {
 public:
  std::string message;
  explicit Error(std::string /*message_in*/);
  std::string toJson() override;
  static Error fromJson(const std::string& /*json_str*/);
};

class UpdateAvailable : public BaseEvent {
 public:
  data::UpdateAvailable update_vailable;
  explicit UpdateAvailable(data::UpdateAvailable ua_in);
  std::string toJson() override;
  static UpdateAvailable fromJson(const std::string& json_str);
};

class DownloadComplete : public BaseEvent {
 public:
  explicit DownloadComplete(data::DownloadComplete dc_in);
  data::DownloadComplete download_complete;
  std::string toJson() override;
  static DownloadComplete fromJson(const std::string& json_str);
};

class InstalledSoftwareNeeded : public BaseEvent {
 public:
  InstalledSoftwareNeeded();
  std::string toJson() override;
};

class UptaneTimestampUpdated : public BaseEvent {
 public:
  UptaneTimestampUpdated();
  std::string toJson() override;
};

class UptaneTargetsUpdated : public BaseEvent {
 public:
  std::vector<Uptane::Target> packages;
  std::string toJson() override;

  explicit UptaneTargetsUpdated(std::vector<Uptane::Target> packages_in);
};
}  // namespace event

#endif
