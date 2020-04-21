#include "image_repo.h"

void ImageRepo::addImage(const std::string &name, Json::Value &target, const std::string &hardware_id,
                         const Delegation &delegation) {
  boost::filesystem::path repo_dir(path_ / ImageRepo::dir);

  boost::filesystem::path targets_path =
      delegation ? ((repo_dir / "delegations") / delegation.name).string() + ".json" : repo_dir / "targets.json";
  Json::Value targets = Utils::parseJSONFile(targets_path)["signed"];
  // TODO: support multiple hardware IDs.
  target["custom"]["hardwareIds"][0] = hardware_id;
  targets["targets"][name] = target;
  targets["version"] = (targets["version"].asUInt()) + 1;

  auto role = delegation ? Uptane::Role(delegation.name, true) : Uptane::Role::Targets();
  std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(role, targets));
  Utils::writeFile(targets_path, signed_targets);
  updateRepo();
}

void ImageRepo::addBinaryImage(const boost::filesystem::path &image_path, const boost::filesystem::path &targetname,
                               const std::string &hardware_id, const std::string &url, const Delegation &delegation) {
  boost::filesystem::path repo_dir(path_ / ImageRepo::dir);
  boost::filesystem::path targets_path = repo_dir / "targets";

  auto targetname_dir = targetname.parent_path();
  boost::filesystem::create_directories(targets_path / targetname_dir);
  boost::filesystem::copy_file(image_path, targets_path / targetname_dir / image_path.filename(),
                               boost::filesystem::copy_option::overwrite_if_exists);

  std::string image = Utils::readFile(image_path);

  Json::Value target;
  target["length"] = Json::UInt64(image.size());
  target["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(image)));
  target["hashes"]["sha512"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(image)));
  target["custom"]["targetFormat"] = "BINARY";
  if (!url.empty()) {
    target["custom"]["uri"] = url;
  }
  addImage(targetname.string(), target, hardware_id, delegation);
}

void ImageRepo::addCustomImage(const std::string &name, const Hash &hash, const uint64_t length,
                               const std::string &hardware_id, const std::string &url, const Delegation &delegation,
                               const Json::Value &custom) {
  Json::Value target;
  target["length"] = Json::UInt(length);
  if (hash.type() == Hash::Type::kSha256) {
    target["hashes"]["sha256"] = hash.HashString();
  } else if (hash.type() == Hash::Type::kSha512) {
    target["hashes"]["sha512"] = hash.HashString();
  }
  target["custom"] = custom;
  if (!url.empty()) {
    target["custom"]["uri"] = url;
  }
  addImage(name, target, hardware_id, delegation);
}

void ImageRepo::addDelegation(const Uptane::Role &name, const Uptane::Role &parent_role, const std::string &path,
                              bool terminating, KeyType key_type) {
  if (keys_.count(name) != 0) {
    throw std::runtime_error("Delegation with the same name already exist.");
  }
  if (Uptane::Role::IsReserved(name.ToString())) {
    throw std::runtime_error("Delegation name " + name.ToString() + " is reserved.");
  }

  boost::filesystem::path repo_dir(path_ / ImageRepo::dir);

  boost::filesystem::path parent_path(repo_dir);
  if (parent_role.IsDelegation()) {
    parent_path = parent_path /= "delegations";
  }
  parent_path = parent_path /= (parent_role.ToString() + ".json");

  if (!boost::filesystem::exists(parent_path)) {
    throw std::runtime_error("Delegation role " + parent_role.ToString() + " does not exist.");
  }

  generateKeyPair(key_type, name);
  Json::Value delegate;
  delegate["_type"] = "Targets";
  delegate["expires"] = expiration_time_;
  delegate["version"] = 1;
  delegate["targets"] = Json::objectValue;

  std::string delegate_signed = Utils::jsonToCanonicalStr(signTuf(name, delegate));
  Utils::writeFile((repo_dir / "delegations" / name.ToString()).string() + ".json", delegate_signed);

  Json::Value parent_notsigned = Utils::parseJSONFile(parent_path)["signed"];

  auto keypair = keys_[name];
  parent_notsigned["delegations"]["keys"][keypair.public_key.KeyId()] = keypair.public_key.ToUptane();
  Json::Value role;
  role["name"] = name.ToString();
  role["keyids"].append(keypair.public_key.KeyId());
  role["paths"].append(path);
  role["threshold"] = 1;
  role["terminating"] = terminating;
  parent_notsigned["delegations"]["roles"].append(role);
  parent_notsigned["version"] = (parent_notsigned["version"].asUInt()) + 1;

  std::string signed_parent = Utils::jsonToCanonicalStr(signTuf(parent_role, parent_notsigned));
  Utils::writeFile(parent_path, signed_parent);
  updateRepo();
}

void ImageRepo::removeDelegationRecursive(const Uptane::Role &name, const Uptane::Role &parent_name) {
  boost::filesystem::path repo_dir(path_ / ImageRepo::dir);
  if (parent_name.IsDelegation()) {
    repo_dir = repo_dir / "delegations";
  }
  Json::Value targets = Utils::parseJSONFile(repo_dir / (parent_name.ToString() + ".json"))["signed"];

  auto keypair = keys_[name];
  targets["delegations"]["keys"].removeMember(
      keypair.public_key.KeyId());  // doesn't do anything if the key doesn't exist
  Json::Value new_roles(Json::arrayValue);
  for (const auto &role : targets["delegations"]["roles"]) {
    auto delegated_name = role["name"].asString();
    if (delegated_name != name.ToString()) {
      new_roles.append(role);
      removeDelegationRecursive(name, Uptane::Role(delegated_name, true));
    }
  }
  targets["delegations"]["roles"] = new_roles;
  targets["version"] = (targets["version"].asUInt()) + 1;
  Utils::writeFile(repo_dir / (parent_name.ToString() + ".json"),
                   Utils::jsonToCanonicalStr(signTuf(parent_name, targets)));
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

  boost::filesystem::path repo_dir(path_ / ImageRepo::dir);

  boost::filesystem::remove(repo_dir / "delegations" / (name.ToString() + ".json"));

  removeDelegationRecursive(name, Uptane::Role::Targets());
  updateRepo();
}

std::vector<std::string> ImageRepo::getDelegationTargets(const Uptane::Role &name) {
  std::vector<std::string> result;
  boost::filesystem::path repo_dir(path_ / ImageRepo::dir);
  auto targets = Utils::parseJSONFile((repo_dir / "delegations") / (name.ToString() + ".json"))["signed"]["targets"];
  for (auto it = targets.begin(); it != targets.end(); ++it) {
    result.push_back(it.key().asString());
  }
  return result;
}
