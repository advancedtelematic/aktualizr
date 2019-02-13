#include "image_repo.h"

void ImageRepo::addImage(const boost::filesystem::path &image_path, const boost::filesystem::path &targetname,
                         const Delegation &delegation) {
  boost::filesystem::path repo_dir(path_ / "repo/image");

  boost::filesystem::path targets_path = repo_dir / "targets";
  boost::filesystem::create_directories(targets_path / "delegations");

  auto targetname_dir = targetname.parent_path();
  boost::filesystem::create_directories(targets_path / targetname_dir);
  boost::filesystem::copy_file(image_path, targets_path / targetname_dir / image_path.filename(),
                               boost::filesystem::copy_option::overwrite_if_exists);

  std::string image = Utils::readFile(image_path);

  Json::Value target;
  target["length"] = Json::UInt64(image.size());
  target["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(image)));
  target["hashes"]["sha512"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(image)));
  addImage(targetname.string(), target, delegation);
}

void ImageRepo::addImage(const std::string &name, const Json::Value &target, const Delegation &delegation) {
  boost::filesystem::path repo_dir(path_ / "repo/image");

  boost::filesystem::path targets_path =
      delegation ? ((repo_dir / "delegations") / delegation.name).string() + ".json" : repo_dir / "targets.json";
  Json::Value targets = Utils::parseJSONFile(targets_path)["signed"];
  targets["targets"][name] = target;
  targets["version"] = (targets["version"].asUInt()) + 1;

  auto role = delegation ? Uptane::Role(delegation.name, true) : Uptane::Role::Targets();
  std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(role, targets));
  Utils::writeFile(targets_path, signed_targets);
  if (!delegation) {
    updateRepo();
  }
}

void ImageRepo::addDelegation(const Uptane::Role &name, const std::string &path, KeyType key_type, bool terminating) {
  if (keys_.count(name) != 0) {
    throw std::runtime_error("Delegation with the same name already exist.");
  }
  if (Uptane::Role::IsReserved(name.ToString())) {
    throw std::runtime_error("Delegation name " + name.ToString() + " is reserved.");
  }
  generateKeyPair(key_type, name);
  Json::Value delegate;
  delegate["_type"] = "Targets";
  delegate["expires"] = expiration_time_;
  delegate["version"] = 1;
  delegate["targets"] = Json::objectValue;

  std::string delegate_signed = Utils::jsonToCanonicalStr(signTuf(name, delegate));
  boost::filesystem::path repo_dir(path_ / "repo/image");
  Utils::writeFile(((repo_dir / "delegations") / name.ToString()).string() + ".json", delegate_signed);

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
  targets_notsigned["version"] = (targets_notsigned["version"].asUInt()) + 1;

  std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets_notsigned));
  Utils::writeFile(repo_dir / "targets.json", signed_targets);
  updateRepo();
}

void ImageRepo::revokeDelegation(const Uptane::Role &name) {
  if (keys_.count(name) == 0) {
    throw std::runtime_error("Delegation does not exist.");
  }
  if (Uptane::Role::IsReserved(name.ToString())) {
    throw std::runtime_error("Delegation name " + name.ToString() + " is reserved.");
  }

  boost::filesystem::path keys_dir = path_ / ("keys/image/" + name.ToString());
  boost::filesystem::remove_all(keys_dir);

  boost::filesystem::path repo_dir(path_ / "repo/image");
  boost::filesystem::remove(repo_dir / "delegations" / (name.ToString() + ".json"));
  Json::Value targets = Utils::parseJSONFile(repo_dir / "targets.json")["signed"];
  auto keypair = keys_[name];
  targets["delegations"]["keys"].removeMember(keypair.public_key.KeyId());
  Json::Value new_roles(Json::arrayValue);
  for (const auto &role : targets["delegations"]["roles"]) {
    if (role["name"].asString() != name.ToString()) {
      new_roles.append(role);
    }
  }
  targets["delegations"]["roles"] = new_roles;
  targets["version"] = (targets["version"].asUInt()) + 1;
  Utils::writeFile(repo_dir / "targets.json", Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets)));
  updateRepo();
}

std::vector<std::string> ImageRepo::getDelegationTargets(const Uptane::Role &name) {
  std::vector<std::string> result;
  boost::filesystem::path repo_dir(path_ / "repo/image");
  auto targets = Utils::parseJSONFile((repo_dir / "delegations") / (name.ToString() + ".json"))["signed"]["targets"];
  for (Json::ValueIterator it = targets.begin(); it != targets.end(); ++it) {
    result.push_back(it.key().asString());
  }
  return result;
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
