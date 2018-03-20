#include "events.h"
namespace event {

Json::Value BaseEvent::toBaseJson() {
  Json::Value json;
  json["variant"] = variant;
  json["fields"] = Json::Value(Json::arrayValue);
  return json;
}

Error::Error(const std::string& message_in) : message(message_in) { variant = "Error"; }
std::string Error::toJson() {
  Json::Value json = BaseEvent::toBaseJson();
  json["fields"].append(message);
  return Json::FastWriter().write(json);
}

Error Error::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return Error(json["fields"][0].asString());
}

UpdateAvailable::UpdateAvailable(const data::UpdateAvailable& ua_in) : update_vailable(ua_in) {
  variant = "UpdateAvailable";
}

std::string UpdateAvailable::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(update_vailable.toJson());
  return Json::FastWriter().write(json);
}

UpdateAvailable UpdateAvailable::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return UpdateAvailable(data::UpdateAvailable::fromJson(Json::FastWriter().write(json["fields"][0])));
}

DownloadComplete::DownloadComplete(const data::DownloadComplete& dc_in) : download_complete(dc_in) {
  variant = "DownloadComplete";
}

std::string DownloadComplete::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(download_complete.toJson());
  return Json::FastWriter().write(json);
}

DownloadComplete DownloadComplete::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return DownloadComplete(data::DownloadComplete::fromJson(Json::FastWriter().write(json["fields"][0])));
}

InstalledSoftwareNeeded::InstalledSoftwareNeeded() { variant = "InstalledSoftwareNeeded"; }

std::string InstalledSoftwareNeeded::toJson() { return Json::FastWriter().write(toBaseJson()); }

UptaneTimestampUpdated::UptaneTimestampUpdated() { variant = "UptaneTimestampUpdated"; }

std::string UptaneTimestampUpdated::toJson() { return Json::FastWriter().write(toBaseJson()); }

UptaneTargetsUpdated::UptaneTargetsUpdated(std::vector<Uptane::Target> packages_in) : packages(packages_in) {
  variant = "UptaneTargetsUpdated";
}

std::string UptaneTargetsUpdated::toJson() { return Json::FastWriter().write(toBaseJson()); }
}  // namespace event
