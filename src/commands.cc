#include "commands.h"
#include <boost/make_shared.hpp>

namespace command {

Json::Value BaseCommand::toJson() {
  Json::Value json;
  json["variant"] = variant;
  json["fields"] = Json::Value(Json::arrayValue);
  return json;
}

boost::shared_ptr<BaseCommand> BaseCommand::fromPicoJson(const picojson::value& json) {
  std::string variant = json.get("variant").to_str();
  std::string data = json.serialize(false);
  if (variant == "Authenticate") {
    return boost::make_shared<Authenticate>();
  } else if (variant == "Shutdown") {
    return boost::make_shared<Shutdown>();
  } else if (variant == "GetUpdateRequests") {
    return boost::make_shared<GetUpdateRequests>();
  } else if (variant == "ListInstalledPackages") {
    return boost::make_shared<ListInstalledPackages>();
  } else if (variant == "ListSystemInfo") {
    return boost::make_shared<ListSystemInfo>();
  } else if (variant == "StartDownload") {
    return boost::make_shared<StartDownload>(StartDownload::fromJson(data));
  } else if (variant == "AbortDownload") {
    return boost::make_shared<AbortDownload>(AbortDownload::fromJson(data));
  } else if (variant == "StartInstall") {
    return boost::make_shared<StartInstall>(StartInstall::fromJson(data));
  } else if (variant == "SendInstalledPackages") {
    return boost::make_shared<SendInstalledPackages>(SendInstalledPackages::fromJson(data));
  } else if (variant == "SendInstalledSoftware") {
    return boost::make_shared<SendInstalledSoftware>(SendInstalledSoftware::fromJson(data));
  } else if (variant == "SendSystemInfo") {
    return boost::make_shared<SendSystemInfo>();
  } else if (variant == "SendUpdateReport") {
    return boost::make_shared<SendUpdateReport>(SendUpdateReport::fromJson(data));
  } else {
    throw std::runtime_error("wrong command variant = " + variant);
  }
  return boost::make_shared<BaseCommand>();
}

Authenticate::Authenticate() { variant = "Authenticate"; }
std::string Authenticate::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

Shutdown::Shutdown() { variant = "Shutdown"; }
std::string Shutdown::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

GetUpdateRequests::GetUpdateRequests() { variant = "GetUpdateRequests"; }
std::string GetUpdateRequests::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

ListInstalledPackages::ListInstalledPackages() { variant = "ListInstalledPackages"; }
std::string ListInstalledPackages::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

ListSystemInfo::ListSystemInfo() { variant = "ListSystemInfo"; }
std::string ListSystemInfo::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

StartDownload::StartDownload(const data::UpdateRequestId& ur_in) : update_request_id(ur_in) {
  variant = "StartDownload";
}

std::string StartDownload::toJson() {
  Json::Value json = BaseCommand::toJson();
  json["fields"].append(update_request_id);
  return Json::FastWriter().write(json);
}

StartDownload StartDownload::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return StartDownload(json["fields"][0].asString());
}

AbortDownload::AbortDownload(const data::UpdateRequestId& ur_in) : update_request_id(ur_in) {
  variant = "AbortDownload";
}

std::string AbortDownload::toJson() {
  Json::Value json = BaseCommand::toJson();
  json["fields"].append(update_request_id);
  return Json::FastWriter().write(json);
}

AbortDownload AbortDownload::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return AbortDownload(json["fields"][0].asString());
}

StartInstall::StartInstall(const data::UpdateRequestId& ur_in) : update_request_id(ur_in) { variant = "StartInstall"; }

std::string StartInstall::toJson() {
  Json::Value json = BaseCommand::toJson();
  json["fields"].append(update_request_id);
  return Json::FastWriter().write(json);
}

StartInstall StartInstall::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  return StartInstall(json["fields"][0].asString());
}

SendInstalledPackages::SendInstalledPackages(const std::vector<data::Package>& packages_in) : packages(packages_in) {
  variant = "SendInstalledPackages";
}

std::string SendInstalledPackages::toJson() {
  Json::Value json = BaseCommand::toJson();
  Json::Value packages_json(Json::arrayValue);
  for (std::vector<data::Package>::iterator it = packages.begin(); it != packages.end(); ++it) {
    packages_json.append(it->toJson());
  }
  json["fields"].append(packages_json);
  return Json::FastWriter().write(json);
}

SendInstalledPackages SendInstalledPackages::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<data::Package> packages;
  for (unsigned int index = 0; index < json["fields"][0].size(); ++index) {
    packages.push_back(data::Package::fromJson(Json::FastWriter().write(json["fields"][0][index])));
  }
  return SendInstalledPackages(packages);
}

SendInstalledSoftware::SendInstalledSoftware(const data::InstalledSoftware& installed_software_in)
    : installed_software(installed_software_in) {
  variant = "SendInstalledSoftware";
}

std::string SendInstalledSoftware::toJson() {
  Json::Value json = BaseCommand::toJson();
  json["fields"].append(installed_software.toJson());
  return Json::FastWriter().write(json);
}

SendInstalledSoftware SendInstalledSoftware::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);

  return SendInstalledSoftware(data::InstalledSoftware::fromJson(Json::FastWriter().write(json["fields"][0])));
}

SendSystemInfo::SendSystemInfo() { variant = "SendSystemInfo"; }
std::string SendSystemInfo::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

SendUpdateReport::SendUpdateReport(const data::UpdateReport& ureport_in) : update_report(ureport_in) {
  variant = "SendUpdateReport";
}

std::string SendUpdateReport::toJson() {
  Json::Value json = BaseCommand::toJson();
  json["fields"].append(update_report.toJson());
  return Json::FastWriter().write(json);
}

SendUpdateReport SendUpdateReport::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);

  return SendUpdateReport(data::UpdateReport::fromJson(Json::FastWriter().write(json["fields"][0])));
}

#ifdef BUILD_OSTREE
OstreeInstall::OstreeInstall(std::vector<OstreePackage> packages_in) : packages(packages_in) {
  variant = "OstreeInstall";
}
#endif
};
