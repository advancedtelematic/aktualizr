#include "director_repo.h"

void DirectorRepo::addTarget(const std::string &target_name, const Json::Value &target, const std::string &hardware_id,
                             const std::string &ecu_serial) {
  const boost::filesystem::path current = path_ / "repo/director/targets.json";
  const boost::filesystem::path staging = path_ / "repo/director/staging/targets.json";

  Json::Value director_targets;
  if (boost::filesystem::exists(staging)) {
    director_targets = Utils::parseJSONFile(staging);
  } else if (boost::filesystem::exists(current)) {
    director_targets = Utils::parseJSONFile(current)["signed"];
  } else {
    throw std::runtime_error(std::string("targets.json not found at ") + staging.c_str() + " or " + current.c_str() +
                             "!");
  }
  director_targets["targets"][target_name] = target;
  director_targets["targets"][target_name]["custom"]["ecuIdentifiers"][ecu_serial]["hardwareId"] = hardware_id;
  director_targets["version"] = (Utils::parseJSONFile(current)["signed"]["version"].asUInt()) + 1;
  Utils::writeFile(staging, Utils::jsonToCanonicalStr(director_targets));
}

void DirectorRepo::signTargets() {
  const boost::filesystem::path current = path_ / "repo/director/targets.json";
  const boost::filesystem::path staging = path_ / "repo/director/staging/targets.json";
  Json::Value targets_unsigned;

  if (boost::filesystem::exists(staging)) {
    targets_unsigned = Utils::parseJSONFile(staging);
  } else if (boost::filesystem::exists(current)) {
    targets_unsigned = Utils::parseJSONFile(current)["signed"];
  } else {
    throw std::runtime_error(std::string("targets.json not found at ") + staging.c_str() + " or " + current.c_str() +
                             "!");
  }

  Utils::writeFile(path_ / "repo/director/targets.json",
                   Utils::jsonToCanonicalStr(signTuf(keys_[Uptane::Role::Targets()], targets_unsigned)));
  boost::filesystem::remove(path_ / "repo/director/staging/targets.json");
}