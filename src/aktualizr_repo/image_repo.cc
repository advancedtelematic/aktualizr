#include "image_repo.h"

void ImageRepo::addImage(const boost::filesystem::path &image_path) {
  boost::filesystem::path repo_dir(path_ / "repo/image");

  boost::filesystem::path targets_path = repo_dir / "targets";
  boost::filesystem::create_directories(targets_path);
  if (image_path != targets_path / image_path.filename()) {
    boost::filesystem::copy_file(image_path, targets_path / image_path.filename(),
                                 boost::filesystem::copy_option::overwrite_if_exists);
  }
  std::string image = Utils::readFile(image_path);

  std::string target_name = image_path.filename().string();
  Json::Value target;
  target["length"] = Json::UInt64(image.size());
  target["hashes"]["sha256"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest(image)));
  target["hashes"]["sha512"] = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha512digest(image)));
  addImage(target_name, target);
}

void ImageRepo::addImage(const std::string &name, const Json::Value &target) {
  boost::filesystem::path repo_dir(path_ / "repo/image");

  Json::Value targets = Utils::parseJSONFile(repo_dir / "targets.json")["signed"];
  targets["targets"][name] = target;
  targets["version"] = (targets["version"].asUInt()) + 1;

  std::string signed_targets = Utils::jsonToCanonicalStr(signTuf(Uptane::Role::Targets(), targets));
  Utils::writeFile(repo_dir / "targets.json", signed_targets);

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

void ImageRepo::addCustomImage(const std::string &name, const Uptane::Hash &hash, const uint64_t length) {
  Json::Value target;
  target["length"] = Json::UInt(length);
  if (hash.type() == Uptane::Hash::Type::kSha256) {
    target["hashes"]["sha256"] = hash.HashString();
  } else if (hash.type() == Uptane::Hash::Type::kSha512) {
    target["hashes"]["sha512"] = hash.HashString();
  }
  addImage(name, target);
}