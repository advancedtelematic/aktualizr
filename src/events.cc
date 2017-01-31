#include "events.h"

namespace event {

Json::Value BaseEvent::toBaseJson() {
  Json::Value json;
  json["variant"] = variant;
  json["fields"] = Json::Value(Json::arrayValue);
  return json;
}

Error::Error(const std::string& message_in) : message(message_in) {
  variant = "Error";
}
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

Authenticated::Authenticated() { variant = "Authenticated"; }
std::string Authenticated::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

NotAuthenticated::NotAuthenticated() { variant = "NotAuthenticated"; }
std::string NotAuthenticated::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

AlreadyAuthenticated::AlreadyAuthenticated() {
  variant = "AlreadyAuthenticated";
}
std::string AlreadyAuthenticated::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

UpdatesReceived::UpdatesReceived(const std::vector<data::UpdateRequest>& ur)
    : update_requests(ur) {
  variant = "UpdatesReceived";
}

std::string UpdatesReceived::toJson() {
  Json::Value json = toBaseJson();
  Json::Value update_requests_json(Json::arrayValue);
  for (std::vector<data::UpdateRequest>::iterator it = update_requests.begin();
       it != update_requests.end(); ++it) {
    update_requests_json.append(it->toJson());
  }
  json["fields"].append(update_requests_json);
  return Json::FastWriter().write(json);
}

UpdatesReceived UpdatesReceived::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<data::UpdateRequest> ur;
  for (unsigned int index = 0; index < json["fields"][0].size(); ++index) {
    ur.push_back(data::UpdateRequest::fromJson(
        Json::FastWriter().write(json["fields"][0][index])));
  }
  return UpdatesReceived(ur);
}

UpdateAvailable::UpdateAvailable(const data::UpdateAvailable& ua_in)
    : update_vailable(ua_in) {
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
  return UpdateAvailable(data::UpdateAvailable::fromJson(
      Json::FastWriter().write(json["fields"][0])));
}

NoUpdateRequests::NoUpdateRequests() { variant = "NoUpdateRequests"; }

std::string NoUpdateRequests::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

FoundInstalledPackages::FoundInstalledPackages(
    const std::vector<data::Package>& packages_in)
    : packages(packages_in) {
  variant = "FoundInstalledPackages";
}

std::string FoundInstalledPackages::toJson() {
  Json::Value json = toBaseJson();
  Json::Value packages_json(Json::arrayValue);
  for (std::vector<data::Package>::iterator it = packages.begin();
       it != packages.end(); ++it) {
    packages_json.append(it->toJson());
  }
  json["fields"].append(packages_json);
  return Json::FastWriter().write(json);
}

FoundInstalledPackages FoundInstalledPackages::fromJson(
    const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<data::Package> packages;
  for (unsigned int index = 0; index < json["fields"][0].size(); ++index) {
    packages.push_back(data::Package::fromJson(
        Json::FastWriter().write(json["fields"][0][index])));
  }
  return FoundInstalledPackages(packages);
}

FoundSystemInfo::FoundSystemInfo(const std::string& info_in) : info(info_in) {
  variant = "FoundSystemInfo";
}

std::string FoundSystemInfo::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(info);
  return Json::FastWriter().write(json);
}

FoundSystemInfo FoundSystemInfo::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return FoundSystemInfo(json["fields"][0].asString());
}

DownloadingUpdate::DownloadingUpdate(const data::UpdateRequestId& ur_in)
    : update_request_id(ur_in) {
  variant = "DownloadingUpdate";
}

std::string DownloadingUpdate::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(update_request_id);
  return Json::FastWriter().write(json);
}

DownloadingUpdate DownloadingUpdate::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return DownloadingUpdate(json["fields"][0].asString());
}

DownloadComplete::DownloadComplete(const data::DownloadComplete& dc_in)
    : download_complete(dc_in) {
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
  return DownloadComplete(data::DownloadComplete::fromJson(
      Json::FastWriter().write(json["fields"][0])));
}

DownloadFailed::DownloadFailed(const data::UpdateRequestId& ur_in,
                               const std::string& message_in)
    : update_request_id(ur_in), message(message_in) {
  variant = "DownloadFailed";
}

std::string DownloadFailed::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(update_request_id);
  json["fields"].append(message);
  return Json::FastWriter().write(json);
}

DownloadFailed DownloadFailed::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);

  data::UpdateRequestId update_request_id = json["fields"][0].asString();
  std::string message = json["fields"][1].asString();

  return DownloadFailed(update_request_id, message);
}

InstallingUpdate::InstallingUpdate(const data::UpdateRequestId& ur_in)
    : update_request_id(ur_in) {
  variant = "InstallingUpdate";
}

std::string InstallingUpdate::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(update_request_id);
  return Json::FastWriter().write(json);
}

InstallingUpdate InstallingUpdate::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);

  return InstallingUpdate(json["fields"][0].asString());
}

InstallComplete::InstallComplete(const data::UpdateReport& ureport_in)
    : update_report(ureport_in) {
  variant = "InstallComplete";
}

InstallComplete InstallComplete::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);

  return InstallComplete(data::UpdateReport::fromJson(
      Json::FastWriter().write(json["fields"][0])));
}

std::string InstallComplete::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(update_report.toJson());
  return Json::FastWriter().write(json);
}

InstallFailed::InstallFailed(const data::UpdateReport& ureport_in)
    : update_report(ureport_in) {
  variant = "InstallFailed";
}

std::string InstallFailed::toJson() {
  Json::Value json = toBaseJson();
  json["fields"].append(update_report.toJson());
  return Json::FastWriter().write(json);
}

InstallFailed InstallFailed::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);

  return InstallFailed(data::UpdateReport::fromJson(
      Json::FastWriter().write(json["fields"][0])));
}

UpdateReportSent::UpdateReportSent() { variant = "UpdateReportSent"; }

std::string UpdateReportSent::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

InstalledPackagesSent::InstalledPackagesSent() {
  variant = "InstalledPackagesSent";
}

std::string InstalledPackagesSent::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

InstalledSoftwareSent::InstalledSoftwareSent() {
  variant = "InstalledSoftwareSent";
}

std::string InstalledSoftwareSent::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

SystemInfoSent::SystemInfoSent() { variant = "SystemInfoSent"; }

std::string SystemInfoSent::toJson() {
  return Json::FastWriter().write(toBaseJson());
}

InstalledSoftwareNeeded::InstalledSoftwareNeeded() {
  variant = "InstalledSoftwareNeeded";
}

std::string InstalledSoftwareNeeded::toJson() {
  return Json::FastWriter().write(toBaseJson());
}
};