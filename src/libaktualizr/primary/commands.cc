#include "commands.h"

#include <utility>

namespace command {

std::string BaseCommand::toJson() {
  Json::Value json = Json::Value();
  return BaseCommand::toJson(json);
}

std::string BaseCommand::toJson(Json::Value json) {
  json["variant"] = variant;
  if (!json.isMember("fields")) {
    json["fields"] = Json::Value(Json::arrayValue);
  }
  return Json::FastWriter().write(json);
}

std::shared_ptr<BaseCommand> BaseCommand::fromPicoJson(const picojson::value& json) {
  std::string variant = json.get("variant").to_str();
  std::string data = json.serialize(false);
  if (variant == "Shutdown") {
    return std::make_shared<Shutdown>();
  }
  if (variant == "SendDeviceData") {
    return std::make_shared<SendDeviceData>();
  }
  if (variant == "FetchMeta") {
    return std::make_shared<FetchMeta>();
  }
  if (variant == "CheckUpdates") {
    return std::make_shared<CheckUpdates>();
  }
  if (variant == "StartDownload") {
    return std::static_pointer_cast<BaseCommand>(std::make_shared<StartDownload>(StartDownload::fromJson(data)));
  }
  if (variant == "UptaneInstall") {
    return std::static_pointer_cast<BaseCommand>(std::make_shared<UptaneInstall>(UptaneInstall::fromJson(data)));
  }
  throw std::runtime_error("wrong command variant = " + variant);

  return std::make_shared<BaseCommand>();
}

Shutdown::Shutdown() { variant = "Shutdown"; }

FetchMeta::FetchMeta() { variant = "FetchMeta"; }

CheckUpdates::CheckUpdates() { variant = "CheckUpdates"; }

SendDeviceData::SendDeviceData() { variant = "SendDeviceData"; }

StartDownload::StartDownload(std::vector<Uptane::Target> updates_in) : updates(std::move(updates_in)) {
  variant = "StartDownload";
}

std::string StartDownload::toJson() {
  Json::Value json;
  Json::Value targets;
  for (const auto& target : updates) {
    targets[target.filename()] = target.toJson();
  }
  json["fields"].append(targets);
  return BaseCommand::toJson(json);
}

StartDownload StartDownload::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<Uptane::Target> updates;
  for (auto it = json["fields"][0].begin(); it != json["fields"][0].end(); ++it) {
    updates.emplace_back(Uptane::Target(it.key().asString(), *it));
  }
  return StartDownload(updates);
}

UptaneInstall::UptaneInstall(std::vector<Uptane::Target> packages_in) : packages(std::move(packages_in)) {
  variant = "UptaneInstall";
}
UptaneInstall UptaneInstall::fromJson(const std::string& json_str) {
  Json::Reader reader;
  Json::Value json;
  reader.parse(json_str, json);
  std::vector<Uptane::Target> updates;
  for (auto it = json["fields"][0].begin(); it != json["fields"][0].end(); ++it) {
    updates.emplace_back(Uptane::Target(it.key().asString(), *it));
  }
  return UptaneInstall(updates);
}

}  // namespace command
