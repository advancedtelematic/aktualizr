#include "imagesrepository.h"

namespace Uptane {

void ImagesRepository::resetMeta() {
  resetRoot();
  targets = Targets();
  snapshot = Snapshot();
  timestamp = TimestampMeta();
}

bool ImagesRepository::verifyTimestamp(const std::string& timestamp_raw) {
  try {
    timestamp = TimestampMeta(RepositoryType::Images, Utils::parseJSON(timestamp_raw), root);  // signature verification
  } catch (Exception e) {
    LOG_ERROR << "Signature verification for timestamp metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

bool ImagesRepository::verifySnapshot(const std::string& snapshot_raw) {
  try {
    bool hash_exists = false;
    for (const auto& it : timestamp.snapshot_hashes()) {
      switch (it.type()) {
        case Hash::Type::kSha256:
          if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(snapshot_raw))) != it) {
            LOG_ERROR << "Hash verification for snapshot metadata failed";
            return false;
          }
          hash_exists = true;
          break;
        case Hash::Type::kSha512:
          if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(snapshot_raw))) != it) {
            LOG_ERROR << "Hash verification for snapshot metadata failed";
            return false;
          }
          hash_exists = true;
          break;
        default:
          break;
      }
    }
    if (!hash_exists) {
      LOG_ERROR << "No hash found for shapshot.json";
      return false;
    }
    snapshot = Snapshot(RepositoryType::Images, Utils::parseJSON(snapshot_raw), root);  // signature verification
    if (snapshot.version() != timestamp.snapshot_version()) {
      return false;
    }
  } catch (Exception e) {
    LOG_ERROR << "Signature verification for snapshot metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

bool ImagesRepository::verifyTargets(const std::string& targets_raw) {
  try {
    bool hash_exists = false;
    for (const auto& it : snapshot.targets_hashes()) {
      switch (it.type()) {
        case Hash::Type::kSha256:
          if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(targets_raw))) != it) {
            LOG_ERROR << "Hash verification for targets metadata failed";
            return false;
          }
          hash_exists = true;
          break;
        case Hash::Type::kSha512:
          if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(targets_raw))) != it) {
            LOG_ERROR << "Hash verification for targets metadata failed";
            return false;
          }
          hash_exists = true;
          break;
        default:
          break;
      }
    }
    if (!hash_exists) {
      LOG_ERROR << "No hash found for targets.json";
      return false;
    }
    targets = Targets(RepositoryType::Images, Utils::parseJSON(targets_raw), root);  // signature verification
    if (targets.version() != snapshot.targets_version()) {
      return false;
    }
  } catch (Exception e) {
    LOG_ERROR << "Signature verification for images targets metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

std::unique_ptr<Uptane::Target> ImagesRepository::getTarget(const Uptane::Target& director_target) {
  auto it = std::find(targets.targets.begin(), targets.targets.end(), director_target);
  if (it == targets.targets.end()) {
    return std::unique_ptr<Uptane::Target>(nullptr);
  } else {
    return std_::make_unique<Uptane::Target>(*it);
  }
}

}  // namespace Uptane
