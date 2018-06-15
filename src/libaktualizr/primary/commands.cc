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

std::shared_ptr<BaseCommand> BaseCommand::fromJson(const Json::Value& json) {
  std::string v = json["variant"].asString();
  BaseCommand* com;

  if (v == "Shutdown") {
    com = new Shutdown(json);
  } else if (v == "SendDeviceData") {
    com = new SendDeviceData(json);
  } else if (v == "FetchMeta") {
    com = new FetchMeta(json);
  } else if (v == "CheckUpdates") {
    com = new CheckUpdates(json);
  } else if (v == "StartDownload") {
    com = new StartDownload(json);
  } else if (v == "UptaneInstall") {
    com = new UptaneInstall(json);
  } else {
    throw std::runtime_error("wrong command variant = " + v);
  }

  return std::shared_ptr<BaseCommand>(com);
}

StartDownload::StartDownload(std::vector<Uptane::Target> updates_in)
    : BaseCommand("StartDownload"), updates(std::move(updates_in)) {}

StartDownload::StartDownload(const Json::Value& json) : StartDownload(std::vector<Uptane::Target>()) {
  for (auto it = json["fields"][0].begin(); it != json["fields"][0].end(); ++it) {
    updates.emplace_back(Uptane::Target(it.key().asString(), *it));
  }
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

UptaneInstall::UptaneInstall(std::vector<Uptane::Target> packages_in)
    : BaseCommand("UptaneInstall"), packages(std::move(packages_in)) {}

UptaneInstall::UptaneInstall(const Json::Value& json) : UptaneInstall(std::vector<Uptane::Target>()) {
  for (auto it = json["fields"][0].begin(); it != json["fields"][0].end(); ++it) {
    packages.emplace_back(Uptane::Target(it.key().asString(), *it));
  }
}

std::string UptaneInstall::toJson() {
  Json::Value json;
  Json::Value targets;
  for (const auto& target : packages) {
    targets[target.filename()] = target.toJson();
  }
  json["fields"].append(targets);
  return BaseCommand::toJson(json);
}

}  // namespace command
