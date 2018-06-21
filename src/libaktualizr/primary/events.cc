#include "events.h"

#include <utility>
namespace event {

std::string BaseEvent::toJson() {
  Json::Value json;
  return BaseEvent::toJson(json);
}

std::string BaseEvent::toJson(Json::Value json) {
  json["variant"] = variant;
  if (!json.isMember("fields")) {
    json["fields"] = Json::Value(Json::arrayValue);
  }
  return Json::FastWriter().write(json);
}

Error::Error(std::string message_in) : message(std::move(message_in)) { variant = "Error"; }
std::string Error::toJson() {
  Json::Value json;
  json["fields"].append(message);
  return BaseEvent::toJson(json);
}

Error Error::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return Error(json["fields"][0].asString());
}

UpdateAvailable::UpdateAvailable(std::vector<Uptane::Target> updates_in) : updates(std::move(updates_in)) {
  variant = "UpdateAvailable";
}

SendDeviceDataComplete::SendDeviceDataComplete() { variant = "SendDeviceDataComplete"; }

PutManifestComplete::PutManifestComplete() { variant = "PutManifestComplete"; }

std::string UpdateAvailable::toJson() {
  Json::Value json;
  Json::Value targets;
  for (const auto& target : updates) {
    targets[target.filename()] = target.toJson();
  }
  json["fields"].append(targets);
  return BaseEvent::toJson(json);
}

UpdateAvailable UpdateAvailable::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<Uptane::Target> updates;
  for (auto it = json["fields"][0].begin(); it != json["fields"][0].end(); ++it) {
    updates.emplace_back(Uptane::Target(it.key().asString(), *it));
  }
  return UpdateAvailable(updates);
}

FetchMetaComplete::FetchMetaComplete() { variant = "FetchMetaComplete"; }

UptaneTimestampUpdated::UptaneTimestampUpdated() { variant = "UptaneTimestampUpdated"; }

DownloadComplete::DownloadComplete(std::vector<Uptane::Target> updates_in) : updates(std::move(updates_in)) {
  variant = "DownloadComplete";
}

std::string DownloadComplete::toJson() {
  Json::Value json;
  Json::Value targets;
  for (const auto& target : updates) {
    targets[target.filename()] = target.toJson();
  }
  json["fields"].append(targets);
  return BaseEvent::toJson(json);
}

DownloadComplete DownloadComplete::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<Uptane::Target> updates;
  for (auto it = json["fields"][0].begin(); it != json["fields"][0].end(); ++it) {
    updates.emplace_back(Uptane::Target(it.key().asString(), *it));
  }
  return DownloadComplete(updates);
}

InstallComplete::InstallComplete() { variant = "InstallComplete"; }

}  // namespace event
