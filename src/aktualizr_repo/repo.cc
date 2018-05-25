#include "repo.h"
#include <ctime>
#include <regex>
#include "crypto/crypto.h"

Repo::Repo(boost::filesystem::path path, const std::string &expires) : path_(std::move(path)) {
  expiration_time_ = getExpirationTime(expires);
}

Json::Value Repo::signTuf(const Json::Value &json, const std::string &key) {
  std::string b64sig = Utils::toBase64(Crypto::Sign(KeyType::kRSA2048, nullptr, key, Json::FastWriter().write(json)));
  Json::Value signature;
  signature["method"] = "rsassa-pss";
  signature["sig"] = b64sig;

  Json::Value signed_data;
  signature["keyid"] = Crypto::getKeyId(key);

  signed_data["signed"] = json;
  signed_data["signatures"].append(signature);
  return signed_data;
}

std::string Repo::getExpirationTime(const std::string &expires) {
  if (expires.size() != 0) {
    std::smatch match;
    std::regex time_pattern("\\d{4}\\-\\d{2}\\-\\d{2}T\\d{2}:\\d{2}:\\d{2}Z");  // NOLINT(modernize-raw-string-literal)
    if (!std::regex_match(expires, time_pattern)) {
      throw std::runtime_error("Expiration time has wrong format");
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

void Repo::generateRepo(const std::string &name) {
  boost::filesystem::path keys_dir(path_ / ("keys/" + name));
  boost::filesystem::create_directories(keys_dir);

  boost::filesystem::path repo_dir(path_ / ("repo/" + name));
  boost::filesystem::create_directories(repo_dir);

  KeyType type{kRSA2048};
  std::string public_key, private_key;
  Crypto::generateKeyPair(type, &public_key, &private_key);
  Utils::writeFile(keys_dir / "private.key", private_key);
  Utils::writeFile(keys_dir / "public.key", public_key);

  Json::Value root;
  root["_type"] = "Root";
  root["expires"] = expiration_time_;
  std::string key_id = Crypto::getKeyId(public_key);
  Json::Value key_item;
  key_item[key_id]["keytype"] = "rsa";
  key_item[key_id]["keyval"]["public"] = public_key;
  root["keys"].append(key_item);
  Json::Value role;
  role["keyids"].append(key_id);
  role["threshold"] = 1;
  root["roles"]["root"] = role;
  root["roles"]["snapshot"] = role;
  root["roles"]["targets"] = role;
  root["roles"]["timestamp"] = role;

  Json::Value signed_root = signTuf(root, private_key);
  Utils::writeFile(repo_dir / "root.json", signed_root);

  Json::Value targets;
  targets["_type"] = "Targets";
  targets["expires"] = expiration_time_;
  targets["version"] = 1;
  targets["targets"] = Json::objectValue;
  Utils::writeFile(repo_dir / "targets.json", signTuf(targets, private_key));

  Json::Value timestamp;
  timestamp["_type"] = "Timestamp";
  timestamp["expires"] = expiration_time_;
  timestamp["version"] = 1;
  Utils::writeFile(repo_dir / "timestamp.json", signTuf(timestamp, private_key));
}

void Repo::generateRepo() {
  generateRepo("director");
  Utils::writeFile(path_ / "repo/director/manifest", std::string());  // just empty file to work with put method
  generateRepo("image");
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
  std::string private_key = Utils::readFile(path_ / "keys/image/private.key");
  Utils::writeFile(repo_dir / "targets.json", signTuf(targets, private_key));
  Json::Value timestamp = Utils::parseJSONFile(repo_dir / "timestamp.json")["signed"];
  timestamp["version"] = (timestamp["version"].asUInt()) + 1;
  Utils::writeFile(repo_dir / "timestamp.json", signTuf(timestamp, private_key));
}

void Repo::copyTarget(const std::string &target_name) {
  Json::Value image_targets = Utils::parseJSONFile(path_ / "repo/image/targets.json")["signed"];
  if (!image_targets["targets"].isMember(target_name)) {
    throw std::runtime_error("No such " + target_name + " target in the image repository");
  }
  Json::Value director_targets = Utils::parseJSONFile(path_ / "repo/director/targets.json")["signed"];
  director_targets["targets"][target_name] = image_targets["targets"][target_name];
  director_targets["version"] = (director_targets["version"].asUInt()) + 1;
  std::string private_key = Utils::readFile(path_ / "keys/director/private.key");
  Utils::writeFile(path_ / "repo/director/targets.json", signTuf(director_targets, private_key));
}
