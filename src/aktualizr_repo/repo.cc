#include "repo.h"
#include <ctime>
#include <regex>
#include "crypto/crypto.h"

Repo::Repo(boost::filesystem::path path, const std::string &expires) : path_(std::move(path)) {
  expiration_time_ = getExpirationTime(expires);
}

Json::Value Repo::signTuf(const std::string &repo_type, const Json::Value &json) {
  std::string private_key = Utils::readFile(path_ / "keys" / repo_type / "private.key");
  std::istringstream key_type_str{Utils::readFile(path_ / "keys" / repo_type / "key_type")};
  KeyType key_type;
  key_type_str >> key_type;

  std::string b64sig = Utils::toBase64(Crypto::Sign(key_type, nullptr, private_key, Json::FastWriter().write(json)));
  Json::Value signature;
  switch (key_type) {
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
  signature["keyid"] = GetPublicKey(repo_type).KeyId();

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

void Repo::generateRepo(const std::string &repo_type, KeyType key_type) {
  boost::filesystem::path keys_dir(path_ / ("keys/" + repo_type));
  boost::filesystem::create_directories(keys_dir);

  boost::filesystem::path repo_dir(path_ / ("repo/" + repo_type));
  boost::filesystem::create_directories(repo_dir);

  std::string public_key_string, private_key;
  Crypto::generateKeyPair(key_type, &public_key_string, &private_key);
  PublicKey public_key(public_key_string, key_type);

  std::stringstream key_str;
  key_str << key_type;
  Utils::writeFile(keys_dir / "private.key", private_key);
  Utils::writeFile(keys_dir / "public.key", public_key_string);
  Utils::writeFile(keys_dir / "key_type", key_str.str());

  Json::Value root;
  root["_type"] = "Root";
  root["expires"] = expiration_time_;
  root["keys"][public_key.KeyId()] = public_key.ToUptane();
  Json::Value role;
  role["keyids"].append(public_key.KeyId());
  role["threshold"] = 1;
  root["roles"]["root"] = role;
  root["roles"]["snapshot"] = role;
  root["roles"]["targets"] = role;
  root["roles"]["timestamp"] = role;

  std::string signed_root = Utils::jsonToStr(signTuf(repo_type, root));
  Utils::writeFile(repo_dir / "root.json", signed_root);
  Utils::writeFile(repo_dir / "1.root.json", signed_root);

  Json::Value targets;
  targets["_type"] = "Targets";
  targets["expires"] = expiration_time_;
  targets["version"] = 1;
  targets["targets"] = Json::objectValue;
  std::string signed_targets = Utils::jsonToStr(signTuf(repo_type, targets));
  Utils::writeFile(repo_dir / "targets.json", signed_targets);

  Json::Value snapshot;
  snapshot["_type"] = "Snapshot";
  snapshot["expires"] = expiration_time_;
  snapshot["version"] = 1;
  snapshot["meta"]["root.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_root)));
  snapshot["meta"]["root.json"]["length"] = static_cast<Json::UInt>(signed_root.length());
  snapshot["meta"]["root.json"]["version"] = 1;
  snapshot["meta"]["targets.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_targets)));
  snapshot["meta"]["targets.json"]["length"] = static_cast<Json::UInt>(signed_targets.length());
  snapshot["meta"]["targets.json"]["version"] = 1;
  std::string signed_snapshot = Utils::jsonToStr(signTuf(repo_type, snapshot));
  Utils::writeFile(repo_dir / "snapshot.json", signed_snapshot);

  Json::Value timestamp;
  timestamp["_type"] = "Timestamp";
  timestamp["expires"] = expiration_time_;
  timestamp["version"] = 1;
  timestamp["meta"]["snapshot.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["length"] = static_cast<Json::UInt>(signed_snapshot.length());
  timestamp["meta"]["snapshot.json"]["version"] = 1;
  Utils::writeFile(repo_dir / "timestamp.json", signTuf(repo_type, timestamp));
}

void Repo::generateRepo(KeyType key_type) {
  generateRepo("director", key_type);
  Utils::writeFile(path_ / "repo/director/manifest", std::string());  // just empty file to work with put method
  generateRepo("image", key_type);
}

void Repo::addImage(const boost::filesystem::path &image_path) {
  boost::filesystem::path repo_dir(path_ / "repo/image");

  boost::filesystem::path targets_path = repo_dir / "targets";
  boost::filesystem::create_directories(targets_path);
  boost::filesystem::copy_file(image_path, targets_path / image_path.filename(),
                               boost::filesystem::copy_option::overwrite_if_exists);
  std::string image = Utils::readFile(image_path);
  std::string hash = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(image)));

  Json::Value targets = Utils::parseJSONFile(repo_dir / "targets.json")["signed"];
  std::string target_name = image_path.filename().string();
  targets["targets"][target_name]["length"] = Json::UInt64(image.size());
  targets["targets"][target_name]["hashes"]["sha256"] = hash;
  targets["version"] = (targets["version"].asUInt()) + 1;

  std::string signed_targets = Utils::jsonToStr(signTuf("image", targets));
  Utils::writeFile(repo_dir / "targets.json", signed_targets);

  Json::Value snapshot = Utils::parseJSONFile(repo_dir / "snapshot.json")["signed"];
  snapshot["version"] = (snapshot["version"].asUInt()) + 1;
  snapshot["meta"]["targets.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_targets)));
  snapshot["meta"]["targets.json"]["length"] = static_cast<Json::UInt>(signed_targets.length());
  snapshot["meta"]["targets.json"]["version"] = targets["version"].asUInt();
  std::string signed_snapshot = Utils::jsonToStr(signTuf("image", snapshot));
  Utils::writeFile(repo_dir / "snapshot.json", signed_snapshot);

  Json::Value timestamp = Utils::parseJSONFile(repo_dir / "timestamp.json")["signed"];
  timestamp["version"] = (timestamp["version"].asUInt()) + 1;
  timestamp["meta"]["snapshot.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["length"] = static_cast<Json::UInt>(signed_snapshot.length());
  timestamp["meta"]["snapshot.json"]["version"] = snapshot["version"].asUInt();
  Utils::writeFile(repo_dir / "timestamp.json", signTuf("image", timestamp));
}

void Repo::addTarget(const std::string &target_name, const std::string &hardware_id, const std::string &ecu_serial) {
  Json::Value image_targets = Utils::parseJSONFile(path_ / "repo/image/targets.json")["signed"];
  if (!image_targets["targets"].isMember(target_name)) {
    throw std::runtime_error("No such " + target_name + " target in the image repository");
  }
  Json::Value director_targets = Utils::parseJSONFile(path_ / "repo/director/targets.json")["signed"];
  if (boost::filesystem::exists(path_ / "repo/director/staging/targets.json")) {
    director_targets = Utils::parseJSONFile(path_ / "repo/director/staging/targets.json");
  } else {
    director_targets = Utils::parseJSONFile(path_ / "repo/director/targets.json")["signed"];
  }
  director_targets["targets"][target_name] = image_targets["targets"][target_name];
  director_targets["targets"][target_name]["custom"]["ecuIdentifiers"][ecu_serial]["hardwareId"] = hardware_id;
  director_targets["version"] =
      (Utils::parseJSONFile(path_ / "repo/director/targets.json")["signed"]["version"].asUInt()) + 1;
  Utils::writeFile(path_ / "repo/director/staging/targets.json", director_targets);
}

void Repo::signTargets() {
  std::string private_key = Utils::readFile(path_ / "keys/director/private.key");
  Json::Value targets_unsigned = Utils::parseJSONFile(path_ / "repo/director/staging/targets.json");
  Utils::writeFile(path_ / "repo/director/targets.json", signTuf("director", targets_unsigned));
  boost::filesystem::remove(path_ / "repo/director/staging/targets.json");
}

PublicKey Repo::GetPublicKey(const std::string &repo_type) const {
  std::string public_key_string = Utils::readFile(path_ / "keys" / repo_type / "public.key");
  std::istringstream key_type_str{Utils::readFile(path_ / "keys" / repo_type / "key_type")};
  KeyType key_type;
  key_type_str >> key_type;

  return PublicKey(public_key_string, key_type);
}
