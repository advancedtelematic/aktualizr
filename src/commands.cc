#include "commands.h"

#include <utility>

namespace command {

Json::Value BaseCommand::toJson() {
  Json::Value json;
  json["variant"] = variant;
  json["fields"] = Json::Value(Json::arrayValue);
  return json;
}

std::shared_ptr<BaseCommand> BaseCommand::fromPicoJson(const picojson::value& json) {
  std::string variant = json.get("variant").to_str();
  std::string data = json.serialize(false);
  if (variant == "Shutdown") {
    return std::make_shared<Shutdown>();
  }
  if (variant == "GetUpdateRequests") {
    return std::make_shared<GetUpdateRequests>();
  }
  if (variant == "StartDownload") {
    return std::make_shared<StartDownload>(StartDownload::fromJson(data));
  }
  if (variant == "AbortDownload") {
    return std::make_shared<AbortDownload>(AbortDownload::fromJson(data));
  }
  if (variant == "SendUpdateReport") {
    return std::make_shared<SendUpdateReport>(SendUpdateReport::fromJson(data));
  }
  throw std::runtime_error("wrong command variant = " + variant);

  return std::make_shared<BaseCommand>();
}

Shutdown::Shutdown() { variant = "Shutdown"; }
std::string Shutdown::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

GetUpdateRequests::GetUpdateRequests() { variant = "GetUpdateRequests"; }
std::string GetUpdateRequests::toJson() { return Json::FastWriter().write(BaseCommand::toJson()); }

StartDownload::StartDownload(data::UpdateRequestId ur_in) : update_request_id(std::move(ur_in)) {
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

AbortDownload::AbortDownload(data::UpdateRequestId ur_in) : update_request_id(std::move(ur_in)) {
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

SendUpdateReport::SendUpdateReport(data::UpdateReport ureport_in) : update_report(std::move(ureport_in)) {
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

UptaneInstall::UptaneInstall(std::vector<Uptane::Target> packages_in) : packages(std::move(packages_in)) {
  variant = "UptaneInstall";
}
}  // namespace command
