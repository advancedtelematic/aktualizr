#include "director_repo.h"

void DirectorRepo::addTarget(const std::string &target_name, const Json::Value &target, const std::string &hardware_id,
                             const std::string &ecu_serial, const std::string &url, const std::string &expires) {
  const boost::filesystem::path current = path_ / DirectorRepo::dir / "targets.json";
  const boost::filesystem::path staging = path_ / DirectorRepo::dir / "staging/targets.json";

  Json::Value director_targets;
  if (boost::filesystem::exists(staging)) {
    director_targets = Utils::parseJSONFile(staging);
  } else if (boost::filesystem::exists(current)) {
    director_targets = Utils::parseJSONFile(current)["signed"];
  } else {
    throw std::runtime_error(std::string("targets.json not found at ") + staging.c_str() + " or " + current.c_str() +
                             "!");
  }
  if (!expires.empty()) {
    director_targets["expires"] = expires;
  }
  director_targets["targets"][target_name] = target;
  director_targets["targets"][target_name]["custom"].removeMember("hardwareIds");
  director_targets["targets"][target_name]["custom"]["ecuIdentifiers"][ecu_serial]["hardwareId"] = hardware_id;
  if (!url.empty()) {
    director_targets["targets"][target_name]["custom"]["uri"] = url;
  } else {
    director_targets["targets"][target_name]["custom"].removeMember("uri");
  }
  director_targets["version"] = (Utils::parseJSONFile(current)["signed"]["version"].asUInt()) + 1;
  Utils::writeFile(staging, Utils::jsonToCanonicalStr(director_targets));
  updateRepo();
}

void DirectorRepo::revokeTargets(const std::vector<std::string> &targets_to_remove) {
  auto targets_path = path_ / DirectorRepo::dir / "targets.json";
  auto targets_unsigned = Utils::parseJSONFile(targets_path)["signed"];

  Json::Value new_targets;
  for (auto it = targets_unsigned["targets"].begin(); it != targets_unsigned["targets"].end(); ++it) {
    if (std::find(targets_to_remove.begin(), targets_to_remove.end(), it.key().asString()) == targets_to_remove.end()) {
      new_targets[it.key().asString()] = *it;
    }
  }
  targets_unsigned["targets"] = new_targets;
  targets_unsigned["version"] = (targets_unsigned["version"].asUInt()) + 1;
  Utils::writeFile(path_ / DirectorRepo::dir / "targets.json",
                   Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets_unsigned)));
  updateRepo();
}

void DirectorRepo::signTargets() {
  const boost::filesystem::path current = path_ / DirectorRepo::dir / "targets.json";
  const boost::filesystem::path staging = path_ / DirectorRepo::dir / "staging/targets.json";
  Json::Value targets_unsigned;

  if (boost::filesystem::exists(staging)) {
    targets_unsigned = Utils::parseJSONFile(staging);
  } else if (boost::filesystem::exists(current)) {
    targets_unsigned = Utils::parseJSONFile(current)["signed"];
  } else {
    throw std::runtime_error(std::string("targets.json not found at ") + staging.c_str() + " or " + current.c_str() +
                             "!");
  }

  Utils::writeFile(path_ / DirectorRepo::dir / "targets.json",
                   Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets_unsigned)));
  boost::filesystem::remove(path_ / DirectorRepo::dir / "staging/targets.json");
  updateRepo();
}

void DirectorRepo::emptyTargets() {
  const boost::filesystem::path current = path_ / DirectorRepo::dir / "targets.json";
  const boost::filesystem::path staging = path_ / DirectorRepo::dir / "staging/targets.json";

  Json::Value targets_current = Utils::parseJSONFile(current);
  Json::Value targets_unsigned;
  targets_unsigned = Utils::parseJSONFile(staging);
  targets_unsigned["_type"] = "Targets";
  targets_unsigned["expires"] = expiration_time_;
  targets_unsigned["version"] = (targets_current["signed"]["version"].asUInt()) + 1;
  targets_unsigned["targets"] = Json::objectValue;
  if (repo_type_ == Uptane::RepositoryType::Director() && !correlation_id_.empty()) {
    targets_unsigned["custom"]["correlationId"] = correlation_id_;
  }
  Utils::writeFile(staging, Utils::jsonToCanonicalStr(targets_unsigned));
}

void DirectorRepo::oldTargets() {
  const boost::filesystem::path current = path_ / DirectorRepo::dir / "targets.json";
  const boost::filesystem::path staging = path_ / DirectorRepo::dir / "staging/targets.json";

  if (!boost::filesystem::exists(current)) {
    throw std::runtime_error(std::string("targets.json not found at ") + current.c_str() + "!");
  }
  Json::Value targets_current = Utils::parseJSONFile(current);
  Json::Value targets_unsigned = targets_current["signed"];
  Utils::writeFile(staging, Utils::jsonToCanonicalStr(targets_unsigned));
}
