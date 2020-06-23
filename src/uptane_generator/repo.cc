#include "repo.h"

#include <array>
#include <ctime>
#include <regex>

#include <libaktualizr/campaign.h>
#include "crypto/crypto.h"
#include "director_repo.h"
#include "image_repo.h"
#include "logging/logging.h"

Repo::Repo(Uptane::RepositoryType repo_type, boost::filesystem::path path, const std::string &expires,
           std::string correlation_id)
    : repo_type_(repo_type), path_(std::move(path)), correlation_id_(std::move(correlation_id)) {
  expiration_time_ = getExpirationTime(expires);
  if (boost::filesystem::exists(path_)) {
    if (boost::filesystem::directory_iterator(path_) != boost::filesystem::directory_iterator()) {
      readKeys();
    }
  }

  if (repo_type_ == Uptane::RepositoryType::Director()) {
    repo_dir_ = path_ / DirectorRepo::dir;
  } else if (repo_type_ == Uptane::RepositoryType::Image()) {
    repo_dir_ = path_ / ImageRepo::dir;
  }
}

void Repo::addDelegationToSnapshot(Json::Value *snapshot, const Uptane::Role &role) {
  boost::filesystem::path repo_dir = repo_dir_;
  if (role.IsDelegation()) {
    repo_dir = repo_dir / "delegations";
  }
  std::string role_file_name = role.ToString() + ".json";

  Json::Value role_json = Utils::parseJSONFile(repo_dir / role_file_name)["signed"];
  std::string signed_role = Utils::readFile(repo_dir / role_file_name);

  (*snapshot)["meta"][role_file_name]["version"] = role_json["version"].asUInt();

  if (role_json["delegations"].isObject()) {
    auto delegations_list = role_json["delegations"]["roles"];

    for (auto it = delegations_list.begin(); it != delegations_list.end(); it++) {
      addDelegationToSnapshot(snapshot, Uptane::Role((*it)["name"].asString(), true));
    }
  }
}

void Repo::updateRepo() {
  const Json::Value old_snapshot = Utils::parseJSONFile(repo_dir_ / "snapshot.json")["signed"];
  Json::Value snapshot;
  snapshot["_type"] = "Snapshot";
  snapshot["expires"] = old_snapshot["expires"];
  snapshot["version"] = (old_snapshot["version"].asUInt()) + 1;

  const Json::Value root = Utils::parseJSONFile(repo_dir_ / "root.json")["signed"];
  snapshot["meta"]["root.json"]["version"] = root["version"].asUInt();

  addDelegationToSnapshot(&snapshot, Uptane::Role::Targets());

  const std::string signed_snapshot = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Snapshot(), snapshot));
  Utils::writeFile(repo_dir_ / "snapshot.json", signed_snapshot);

  Json::Value timestamp = Utils::parseJSONFile(repo_dir_ / "timestamp.json")["signed"];
  timestamp["version"] = (timestamp["version"].asUInt()) + 1;
  timestamp["meta"]["snapshot.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["hashes"]["sha512"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["length"] = static_cast<Json::UInt>(signed_snapshot.length());
  timestamp["meta"]["snapshot.json"]["version"] = snapshot["version"].asUInt();
  Utils::writeFile(repo_dir_ / "timestamp.json",
                   Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Timestamp(), timestamp)));
}

Json::Value Repo::signTuf(const Uptane::Role &role, const Json::Value &json) {
  auto key = keys_[role];
  std::string b64sig =
      Utils::toBase64(Crypto::Sign(key.public_key.Type(), nullptr, key.private_key, Utils::jsonToCanonicalStr(json)));
  Json::Value signature;
  switch (key.public_key.Type()) {
    case KeyType::kRSA2048:
    case KeyType::kRSA3072:
    case KeyType::kRSA4096:
      signature["method"] = "rsassa-pss";
      break;
    case KeyType::kED25519:
      signature["method"] = "ed25519";
      break;
    default:
      throw std::runtime_error("Unknown key type");
  }
  signature["sig"] = b64sig;

  Json::Value signed_data;
  signature["keyid"] = key.public_key.KeyId();

  signed_data["signed"] = json;
  signed_data["signatures"].append(signature);
  return signed_data;
}

std::string Repo::getExpirationTime(const std::string &expires) {
  if (expires.size() != 0) {
    std::smatch match;
    std::regex time_pattern("\\d{4}\\-\\d{2}\\-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z");  // NOLINT(modernize-raw-string-literal)
    if (!std::regex_match(expires, time_pattern)) {
      throw std::runtime_error("Expiration date has wrong format\n date should be in ISO 8601 UTC format");
    }
    return expires;
  } else {
    time_t raw_time;
    struct tm time_struct {};
    time(&raw_time);
    gmtime_r(&raw_time, &time_struct);
    time_struct.tm_year += 3;
    std::array<char, 22> formatted{};
    strftime(formatted.data(), formatted.size(), "%Y-%m-%dT%H:%M:%SZ", &time_struct);
    return formatted.data();
  }
}
void Repo::generateKeyPair(KeyType key_type, const Uptane::Role &key_name) {
  boost::filesystem::path keys_dir = path_ / ("keys/" + repo_type_.toString() + "/" + key_name.ToString());
  boost::filesystem::create_directories(keys_dir);

  std::string public_key_string;
  std::string private_key;
  if (!Crypto::generateKeyPair(key_type, &public_key_string, &private_key)) {
    throw std::runtime_error("Key generation failure");
  }
  PublicKey public_key(public_key_string, key_type);

  std::stringstream key_str;
  key_str << key_type;

  Utils::writeFile(keys_dir / "private.key", private_key);
  Utils::writeFile(keys_dir / "public.key", public_key_string);
  Utils::writeFile(keys_dir / "key_type", key_str.str());

  keys_[key_name] = KeyPair(public_key, private_key);
}

void Repo::generateRepoKeys(KeyType key_type) {
  generateKeyPair(key_type, Uptane::Role::Root());
  generateKeyPair(key_type, Uptane::Role::Snapshot());
  generateKeyPair(key_type, Uptane::Role::Targets());
  generateKeyPair(key_type, Uptane::Role::Timestamp());
}

void Repo::generateRepo(KeyType key_type) {
  generateRepoKeys(key_type);

  boost::filesystem::create_directories(repo_dir_);
  Json::Value root;
  root["_type"] = "Root";
  root["expires"] = expiration_time_;
  root["version"] = 1;
  for (auto const &keypair : keys_) {
    root["keys"][keypair.second.public_key.KeyId()] = keypair.second.public_key.ToUptane();
  }

  Json::Value role;
  role["threshold"] = 1;

  role["keyids"].append(keys_[Uptane::Role::Root()].public_key.KeyId());
  root["roles"]["root"] = role;

  role["keyids"].clear();
  role["keyids"].append(keys_[Uptane::Role::Snapshot()].public_key.KeyId());
  root["roles"]["snapshot"] = role;

  role["keyids"].clear();
  role["keyids"].append(keys_[Uptane::Role::Targets()].public_key.KeyId());
  root["roles"]["targets"] = role;

  role["keyids"].clear();
  role["keyids"].append(keys_[Uptane::Role::Timestamp()].public_key.KeyId());
  root["roles"]["timestamp"] = role;

  const std::string signed_root = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Root(), root));
  Utils::writeFile(repo_dir_ / "root.json", signed_root);
  Utils::writeFile(repo_dir_ / "1.root.json", signed_root);

  Json::Value targets;
  targets["_type"] = "Targets";
  targets["expires"] = expiration_time_;
  targets["version"] = 1;
  targets["targets"] = Json::objectValue;
  if (repo_type_ == Uptane::RepositoryType::Director() && correlation_id_ != "") {
    targets["custom"]["correlationId"] = correlation_id_;
  }
  const std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets));
  Utils::writeFile(repo_dir_ / "targets.json", signed_targets);

  Json::Value snapshot;
  snapshot["_type"] = "Snapshot";
  snapshot["expires"] = expiration_time_;
  snapshot["version"] = 1;
  snapshot["meta"]["root.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_root)));
  snapshot["meta"]["root.json"]["hashes"]["sha512"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_root)));
  snapshot["meta"]["root.json"]["length"] = static_cast<Json::UInt>(signed_root.length());
  snapshot["meta"]["root.json"]["version"] = 1;
  snapshot["meta"]["targets.json"]["version"] = 1;
  std::string signed_snapshot = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Snapshot(), snapshot));
  Utils::writeFile(repo_dir_ / "snapshot.json", signed_snapshot);

  Json::Value timestamp;
  timestamp["_type"] = "Timestamp";
  timestamp["expires"] = expiration_time_;
  timestamp["version"] = 1;
  timestamp["meta"]["snapshot.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["hashes"]["sha512"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["length"] = static_cast<Json::UInt>(signed_snapshot.length());
  timestamp["meta"]["snapshot.json"]["version"] = 1;
  Utils::writeFile(repo_dir_ / "timestamp.json",
                   Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Timestamp(), timestamp)));
  if (repo_type_ == Uptane::RepositoryType::Director()) {
    Utils::writeFile(path_ / DirectorRepo::dir / "manifest", std::string());  // just empty file to work with put method
  }
}

void Repo::generateCampaigns() const {
  std::vector<campaign::Campaign> campaigns;
  campaigns.resize(1);
  auto &c = campaigns[0];

  c.name = "campaign1";
  c.id = "c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493";
  c.size = 62470;
  c.autoAccept = true;
  c.description = "this is my message to show on the device";
  c.estInstallationDuration = 10;
  c.estPreparationDuration = 20;

  Json::Value json;
  campaign::Campaign::JsonFromCampaigns(campaigns, json);

  Utils::writeFile(path_ / "campaigns.json", Utils::jsonToCanonicalStr(json));
}

Json::Value Repo::getTarget(const std::string &target_name) {
  const Json::Value image_targets = Utils::parseJSONFile(repo_dir_ / "targets.json")["signed"];
  if (image_targets["targets"].isMember(target_name)) {
    return image_targets["targets"][target_name];
  } else if (repo_type_ == Uptane::RepositoryType::Image()) {
    if (!boost::filesystem::is_directory(repo_dir_ / "delegations")) {
      return {};
    }
    for (auto &p : boost::filesystem::directory_iterator(repo_dir_ / "delegations")) {
      if (Uptane::Role::IsReserved(p.path().stem().string())) {
        continue;
      }
      auto targets = Utils::parseJSONFile(p)["signed"];
      if (targets["targets"].isMember(target_name)) {
        return targets["targets"][target_name];
      }
    }
  }
  return {};
}

void Repo::readKeys() {
  auto keys_path = path_ / "keys" / repo_type_.toString();
  for (auto &p : boost::filesystem::directory_iterator(keys_path)) {
    std::string public_key_string = Utils::readFile(p / "public.key");
    std::istringstream key_type_str(Utils::readFile(p / "key_type"));
    KeyType key_type;
    key_type_str >> key_type;
    std::string private_key_string(Utils::readFile(p / "private.key"));
    auto name = p.path().filename().string();
    keys_[Uptane::Role(name, !Uptane::Role::IsReserved(name))] =
        KeyPair(PublicKey(public_key_string, key_type), private_key_string);
  }
}

void Repo::refresh(const Uptane::Role &role) {
  boost::filesystem::path meta_path = repo_dir_;

  if (repo_type_ == Uptane::RepositoryType::Director() &&
      (role == Uptane::Role::Timestamp() || role == Uptane::Role::Snapshot())) {
    throw std::runtime_error("The " + role.ToString() + " in the Director repo is not currently supported.");
  }

  if (role == Uptane::Role::Root()) {
    meta_path /= "root.json";
  } else if (role == Uptane::Role::Timestamp()) {
    meta_path /= "timestamp.json";
  } else if (role == Uptane::Role::Snapshot()) {
    meta_path /= "snapshot.json";
  } else if (role == Uptane::Role::Targets()) {
    meta_path /= "targets.json";
  } else {
    throw std::runtime_error("Refreshing custom role " + role.ToString() + " is not currently supported.");
  }

  // The only interesting part here is to increment the version. It could be
  // interesting to allow changing the expiry, too.
  Json::Value meta_raw = Utils::parseJSONFile(meta_path)["signed"];
  const unsigned version = meta_raw["version"].asUInt() + 1;

  auto current_expire_time = TimeStamp(meta_raw["expires"].asString());

  if (current_expire_time.IsExpiredAt(TimeStamp::Now())) {
    time_t new_expiration_time;
    std::time(&new_expiration_time);
    new_expiration_time += 60 * 60;  // make it valid for the next hour
    struct tm new_expiration_time_str {};
    gmtime_r(&new_expiration_time, &new_expiration_time_str);

    meta_raw["expires"] = TimeStamp(new_expiration_time_str).ToString();
  }
  meta_raw["version"] = version;
  const std::string signed_meta = Utils::jsonToCanonicalStr(signTuf(role, meta_raw));
  Utils::writeFile(meta_path, signed_meta);

  // Write a new numbered version of the Root if relevant.
  if (role == Uptane::Role::Root()) {
    std::stringstream root_name;
    root_name << version << ".root.json";
    Utils::writeFile(repo_dir_ / root_name.str(), signed_meta);
  }

  updateRepo();
}

Delegation::Delegation(const boost::filesystem::path &repo_path, std::string delegation_name)
    : name(std::move(delegation_name)) {
  if (Uptane::Role::IsReserved(name)) {
    throw std::runtime_error("Delegation name " + name + " is reserved.");
  }
  boost::filesystem::path delegation_path(((repo_path / ImageRepo::dir / "delegations") / name).string() + ".json");
  boost::filesystem::path targets_path(repo_path / ImageRepo::dir / "targets.json");
  if (!boost::filesystem::exists(delegation_path) || !boost::filesystem::exists(targets_path)) {
    throw std::runtime_error(std::string("delegation ") + delegation_path.string() + " does not exist");
  }

  pattern = findPatternInTree(repo_path, name, Utils::parseJSONFile(targets_path)["signed"]);

  if (pattern.empty()) {
    throw std::runtime_error("Could not find delegation role in the delegation tree");
  }
}

std::string Delegation::findPatternInTree(const boost::filesystem::path &repo_path, const std::string &name,
                                          const Json::Value &targets_json) {
  Json::Value delegations = targets_json["delegations"];
  for (const auto &role : delegations["roles"]) {
    auto role_name = role["name"].asString();
    if (role_name == name) {
      auto pattern = role["paths"][0].asString();
      if (pattern.back() == '/') {
        pattern.append("**");
      }
      return pattern;
    } else {
      auto pattern = findPatternInTree(
          repo_path, name,
          Utils::parseJSONFile((repo_path / ImageRepo::dir / "delegations") / (role_name + ".json"))["signed"]);
      if (!pattern.empty()) {
        return pattern;
      }
    }
  }

  return "";
}
