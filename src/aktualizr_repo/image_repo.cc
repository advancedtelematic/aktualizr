#include "image_repo.h"

void ImageRepo::addImage(const boost::filesystem::path &image_path, const Delegation &delegation) {
  boost::filesystem::path repo_dir(path_ / "repo/image");

  boost::filesystem::path targets_path = repo_dir / "targets";
  boost::filesystem::create_directories(targets_path);

  auto image_dir = image_path.parent_path();
  boost::filesystem::create_directories(targets_path / image_dir);
  boost::filesystem::copy_file(image_path, targets_path / image_dir / image_path.filename(),
                               boost::filesystem::copy_option::overwrite_if_exists);

  std::string image = Utils::readFile(image_path);

  std::string target_name = delegation ? image_path.string() : image_path.filename().string();
  Json::Value target;
  target["length"] = Json::UInt64(image.size());
  target["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(image)));
  target["hashes"]["sha512"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(image)));
  addImage(target_name, target, delegation);
}

void ImageRepo::addImage(const std::string &name, const Json::Value &target, const Delegation &delegation) {
  boost::filesystem::path repo_dir(path_ / "repo/image");

  boost::filesystem::path targets_path =
      delegation ? (repo_dir / delegation.name).string() + ".json" : repo_dir / "targets.json";
  Json::Value targets = Utils::parseJSONFile(targets_path)["signed"];
  targets["targets"][name] = target;
  targets["version"] = (targets["version"].asUInt()) + 1;

  auto role = delegation ? Uptane::Role(delegation.name, true) : Uptane::Role::Targets();
  std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(role, targets));
  Utils::writeFile(targets_path, signed_targets);

  Json::Value snapshot = Utils::parseJSONFile(repo_dir / "snapshot.json")["signed"];
  snapshot["version"] = (snapshot["version"].asUInt()) + 1;
  snapshot["meta"]["targets.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_targets)));
  snapshot["meta"]["targets.json"]["hashes"]["sha512"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_targets)));
  snapshot["meta"]["targets.json"]["length"] = static_cast<Json::UInt>(signed_targets.length());
  snapshot["meta"]["targets.json"]["version"] = targets["version"].asUInt();
  std::string signed_snapshot = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Snapshot(), snapshot));
  Utils::writeFile(repo_dir / "snapshot.json", signed_snapshot);

  Json::Value timestamp = Utils::parseJSONFile(repo_dir / "timestamp.json")["signed"];
  timestamp["version"] = (timestamp["version"].asUInt()) + 1;
  timestamp["meta"]["snapshot.json"]["hashes"]["sha256"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["hashes"]["sha512"] =
      boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(signed_snapshot)));
  timestamp["meta"]["snapshot.json"]["length"] = static_cast<Json::UInt>(signed_snapshot.length());
  timestamp["meta"]["snapshot.json"]["version"] = snapshot["version"].asUInt();
  Utils::writeFile(repo_dir / "timestamp.json",
                   Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Timestamp(), timestamp)));
}

void ImageRepo::addDelegation(const Uptane::Role &name, const std::string &path, KeyType key_type, bool terminating) {
  if (keys_.count(name) != 0) {
    throw std::runtime_error("Delegation with the same name already exist.");
  }
  if (Delegation::isBadName(name.ToString())) {
    throw std::runtime_error("Delegation with the wrong name, this name is reserved.");
  }
  generateKeyPair(key_type, name);
  Json::Value delegate;
  delegate["_type"] = "Targets";
  delegate["expires"] = expiration_time_;
  delegate["version"] = 1;
  delegate["targets"] = Json::objectValue;

  std::string delegate_signed = Utils::jsonToCanonicalStr(signTuf(name, delegate));
  boost::filesystem::path repo_dir(path_ / "repo/image");
  Utils::writeFile((repo_dir / name.ToString()).string() + ".json", delegate_signed);

  Json::Value targets_notsigned = Utils::parseJSONFile(repo_dir / "targets.json")["signed"];

  auto keypair = keys_[name];
  targets_notsigned["delegations"]["keys"][keypair.public_key.KeyId()] = keypair.public_key.ToUptane();
  Json::Value role;
  role["name"] = name.ToString();
  role["keyids"].append(keypair.public_key.KeyId());
  role["paths"].append(path);
  role["threshold"] = 1;
  role["terminating"] = terminating;
  targets_notsigned["delegations"]["roles"].append(role);

  std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets_notsigned));
  Utils::writeFile(repo_dir / "targets.json", signed_targets);
}

void ImageRepo::addCustomImage(const std::string &name, const Uptane::Hash &hash, const uint64_t length,
                               const Delegation &delegation) {
  Json::Value target;
  target["length"] = Json::UInt(length);
  if (hash.type() == Uptane::Hash::Type::kSha256) {
    target["hashes"]["sha256"] = hash.HashString();
  } else if (hash.type() == Uptane::Hash::Type::kSha512) {
    target["hashes"]["sha512"] = hash.HashString();
  }
  addImage(name, target, delegation);
}