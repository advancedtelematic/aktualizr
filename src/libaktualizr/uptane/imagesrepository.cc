#include "imagesrepository.h"

namespace Uptane {

void ImagesRepository::resetMeta() {
  resetRoot();
  targets.reset();
  snapshot = Snapshot();
  timestamp = TimestampMeta();
}

bool ImagesRepository::verifyTimestamp(const std::string& timestamp_raw) {
  try {
    // Verify the signature:
    timestamp =
        TimestampMeta(RepositoryType::Image(), Utils::parseJSON(timestamp_raw), std::make_shared<MetaWithKeys>(root));
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for timestamp metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

bool ImagesRepository::verifySnapshot(const std::string& snapshot_raw) {
  try {
    const std::string canonical = Utils::jsonToCanonicalStr(Utils::parseJSON(snapshot_raw));
    bool hash_exists = false;
    for (const auto& it : timestamp.snapshot_hashes()) {
      switch (it.type()) {
        case Hash::Type::kSha256:
          if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(canonical))) != it) {
            LOG_ERROR << "Hash verification for snapshot metadata failed";
            return false;
          }
          hash_exists = true;
          break;
        case Hash::Type::kSha512:
          if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(canonical))) != it) {
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
    // Verify the signature:
    snapshot = Snapshot(RepositoryType::Image(), Utils::parseJSON(snapshot_raw), std::make_shared<MetaWithKeys>(root));
    if (snapshot.version() != timestamp.snapshot_version()) {
      return false;
    }
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for snapshot metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

bool ImagesRepository::verifyRoleHashes(const std::string& role_data, const Uptane::Role& role) const {
  const std::string canonical = Utils::jsonToCanonicalStr(Utils::parseJSON(role_data));
  bool hash_exists = false;
  for (const auto& it : snapshot.role_hashes(role)) {
    switch (it.type()) {
      case Hash::Type::kSha256:
        if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(canonical))) != it) {
          LOG_ERROR << "Hash verification for " << role.ToString() << " metadata failed";
          return false;
        }
        hash_exists = true;
        break;
      case Hash::Type::kSha512:
        if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(canonical))) != it) {
          LOG_ERROR << "Hash verification for " << role.ToString() << " metadata failed";
          return false;
        }
        hash_exists = true;
        break;
      default:
        break;
    }
  }

  if (!hash_exists) {
    LOG_ERROR << "No hash found for " << role.ToString() << " role";
  }

  return hash_exists;
}

int ImagesRepository::getRoleVersion(const Uptane::Role& role) const { return snapshot.role_version(role); }

int64_t ImagesRepository::getRoleSize(const Uptane::Role& role) const { return snapshot.role_size(role); }

bool ImagesRepository::verifyTargets(const std::string& targets_raw) {
  try {
    if (!verifyRoleHashes(targets_raw, Uptane::Role::Targets())) {
      return false;
    }

    auto targets_json = Utils::parseJSON(targets_raw);

    // Verify the signature:
    auto signer = std::make_shared<MetaWithKeys>(root);
    targets = std::make_shared<Uptane::Targets>(
        Targets(RepositoryType::Image(), Uptane::Role::Targets(), targets_json, signer));

    if (targets->version() != snapshot.role_version(Uptane::Role::Targets())) {
      return false;
    }
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for images targets metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

std::shared_ptr<Uptane::Targets> ImagesRepository::verifyDelegation(const std::string& delegation_raw,
                                                                    const Uptane::Role& role,
                                                                    const Targets& parent_target) {
  try {
    const Json::Value delegation_json = Utils::parseJSON(delegation_raw);
    const std::string canonical = Utils::jsonToCanonicalStr(delegation_json);

    // Verify the signature:
    auto signer = std::make_shared<MetaWithKeys>(parent_target);
    return std::make_shared<Uptane::Targets>(Targets(RepositoryType::Image(), role, delegation_json, signer));
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for images delegated targets metadata failed";
    throw e;
  }

  return std::shared_ptr<Uptane::Targets>(nullptr);
}

}  // namespace Uptane
