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

NoUpdateAvailable::NoUpdateAvailable() { variant = "NoUpdateAvailable"; }

UpdateAvailable::UpdateAvailable(std::vector<Uptane::Target> updates_in, unsigned int ecus_count_in)
    : updates(std::move(updates_in)), ecus_count(ecus_count_in) {
  variant = "UpdateAvailable";
}

SendDeviceDataComplete::SendDeviceDataComplete() { variant = "SendDeviceDataComplete"; }

PutManifestComplete::PutManifestComplete() { variant = "PutManifestComplete"; }

std::string UpdateAvailable::toJson() {
  Json::Value json;
  Json::Value targets;
  for (const auto& target : updates) {
    targets[target.filename()] = target.toDebugJson();
  }
  json["fields"].append(targets);
  json["fields"].append(ecus_count);
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
  return UpdateAvailable(updates, json["fields"][1].asUInt());
}

FetchMetaComplete::FetchMetaComplete() { variant = "FetchMetaComplete"; }

NothingToDownload::NothingToDownload() { variant = "NothingToDownload"; }

DownloadProgressReport::DownloadProgressReport(Uptane::Target target_in, std::string description_in,
                                               unsigned int progress_in)
    : target{std::move(target_in)}, description{std::move(description_in)}, progress{progress_in} {
  variant = "DownloadProgressReport";
}

std::string DownloadProgressReport::toJson() {
  Json::Value json;
  json["fields"].append(target.toDebugJson());
  json["fields"].append(progress);
  return BaseEvent::toJson(json);
}

AllDownloadsComplete::AllDownloadsComplete(std::vector<Uptane::Target> updates_in) : updates(std::move(updates_in)) {
  variant = "AllDownloadsComplete";
}

std::string AllDownloadsComplete::toJson() {
  Json::Value json;
  Json::Value targets;
  for (const auto& target : updates) {
    targets[target.filename()] = target.toDebugJson();
  }
  json["fields"].append(targets);
  return BaseEvent::toJson(json);
}

DownloadTargetComplete::DownloadTargetComplete(Uptane::Target update_in) : update(std::move(update_in)) {
  variant = "DownloadTargetComplete";
}

std::string DownloadTargetComplete::toJson() {
  Json::Value json;
  json["fields"].append(update.toDebugJson());
  return BaseEvent::toJson(json);
}

AllDownloadsComplete AllDownloadsComplete::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<Uptane::Target> updates;
  for (auto it = json["fields"][0].begin(); it != json["fields"][0].end(); ++it) {
    updates.emplace_back(Uptane::Target(it.key().asString(), *it));
  }
  return AllDownloadsComplete(updates);
}

InstallStarted::InstallStarted(Uptane::EcuSerial serial_in) : serial(std::move(serial_in)) {
  variant = "InstallStarted";
}

InstallTargetComplete::InstallTargetComplete(Uptane::EcuSerial serial_in, bool success_in)
    : serial(std::move(serial_in)), success(success_in) {
  variant = "InstallTargetComplete";
}

AllInstallsComplete::AllInstallsComplete() { variant = "AllInstallsComplete"; }

CampaignCheckComplete::CampaignCheckComplete(std::vector<campaign::Campaign> campaigns_in)
    : campaigns(std::move(campaigns_in)) {
  variant = "CampaignCheckComplete";
}

CampaignAcceptComplete::CampaignAcceptComplete() { variant = "CampaignAcceptComplete"; }

}  // namespace event
