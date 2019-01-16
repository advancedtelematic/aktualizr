#include <ctime>
#include <regex>
#include "crypto/crypto.h"
#include "logging/logging.h"

#include "repo.h"

Repo::Repo(Uptane::RepositoryType repo_type, boost::filesystem::path path, const std::string &expires,
           std::string correlation_id)
    : repo_type_(repo_type), path_(std::move(path)), correlation_id_(std::move(correlation_id)) {
  expiration_time_ = getExpirationTime(expires);
  if (boost::filesystem::exists(path_)) {
    if (boost::filesystem::directory_iterator(path_) != boost::filesystem::directory_iterator()) {
      readKeys();
    }
  }
}

Json::Value Repo::signTuf(const Uptane::Role &role, const Json::Value &json) {
  auto key = keys_[role];
  std::string b64sig =
      Utils::toBase64(Crypto::Sign(key.public_key.Type(), nullptr, key.private_key, Json::FastWriter().write(json)));
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
    char formatted[22];
    strftime(formatted, 22, "%Y-%m-%dT%H:%M:%SZ", &time_struct);
    return formatted;
  }
}
void Repo::generateKeyPair(KeyType key_type, const Uptane::Role &key_name) {
  boost::filesystem::path keys_dir = path_ / ("keys/" + repo_type_.toString() + "/" + key_name.ToString());
  boost::filesystem::create_directories(keys_dir);

  std::string public_key_string, private_key;
  Crypto::generateKeyPair(key_type, &public_key_string, &private_key);
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

  boost::filesystem::path repo_dir(path_ / ("repo/" + repo_type_.toString()));
  boost::filesystem::create_directories(repo_dir);
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

  std::string signed_root = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Root(), root));
  Utils::writeFile(repo_dir / "root.json", signed_root);
  Utils::writeFile(repo_dir / "1.root.json", signed_root);

  Json::Value targets;
  targets["_type"] = "Targets";
  targets["expires"] = expiration_time_;
  targets["version"] = 1;
  targets["targets"] = Json::objectValue;
  LOG_ERROR << "repo: " << repo_type_.toString();
  if (repo_type_ == Uptane::RepositoryType::Director() && correlation_id_ != "") {
    targets["custom"]["correlationId"] = correlation_id_;
  }
  std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets));
  Utils::writeFile(repo_dir / "targets.json", signed_targets);

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
  snapshot["meta"]["targets.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_targets)));
  snapshot["meta"]["targets.json"]["hashes"]["sha512"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_targets)));
  snapshot["meta"]["targets.json"]["length"] = static_cast<Json::UInt>(signed_targets.length());
  snapshot["meta"]["targets.json"]["version"] = 1;
  std::string signed_snapshot = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Snapshot(), snapshot));
  Utils::writeFile(repo_dir / "snapshot.json", signed_snapshot);

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
  Utils::writeFile(repo_dir / "timestamp.json",
                   Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Timestamp(), timestamp)));
  if (repo_type_ == Uptane::RepositoryType::Director()) {
    Utils::writeFile(path_ / "repo/director/manifest", std::string());  // just empty file to work with put method
  }
}

Json::Value Repo::getTarget(const std::string &target_name) {
  const Json::Value image_targets =
      Utils::parseJSONFile(path_ / "repo" / repo_type_.toString() / "targets.json")["signed"];
  return image_targets["targets"][target_name];
}

void Repo::readKeys() {
  auto keys_path = path_ / "keys" / repo_type_.toString();
  for (auto &p : boost::filesystem::directory_iterator(keys_path)) {
    std::string public_key_string = Utils::readFile(p / "public.key");
    std::istringstream key_type_str(Utils::readFile(p / "key_type"));
    KeyType key_type;
    key_type_str >> key_type;
    std::string private_key_string(Utils::readFile(p / "private.key"));
    keys_[Uptane::Role(p.path().filename().string())] =
        KeyPair(PublicKey(public_key_string, key_type), private_key_string);
  }
}
