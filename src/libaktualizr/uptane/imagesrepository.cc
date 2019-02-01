#include "imagesrepository.h"

#include <fnmatch.h>

namespace Uptane {

void ImagesRepository::resetMeta() {
  resetRoot();
  targets.clear();
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

bool ImagesRepository::verifyTargets(const std::string& targets_raw, const Uptane::Role& role) {
  try {
    const Json::Value targets_json = Utils::parseJSON(targets_raw);
    const std::string canonical = Utils::jsonToCanonicalStr(targets_json);
    // Delegated targets do not have their hashes stored in their parent.
    if (role == Uptane::Role::Targets()) {
      bool hash_exists = false;
      for (const auto& it : snapshot.targets_hashes()) {
        switch (it.type()) {
          case Hash::Type::kSha256:
            if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(canonical))) != it) {
              LOG_ERROR << "Hash verification for targets metadata failed";
              return false;
            }
            hash_exists = true;
            break;
          case Hash::Type::kSha512:
            if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(canonical))) != it) {
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
    }

    // Verify the signature:
    std::shared_ptr<MetaWithKeys> signer;
    if (role == Uptane::Role::Targets()) {
      signer = std::make_shared<MetaWithKeys>(root);
    } else {
      // TODO: support nested delegations here. This currently assumes that all
      // delegated targets are signed by the top-level targets.
      signer = std::make_shared<MetaWithKeys>(targets["targets"]);
    }
    targets[role.ToString()] = Targets(RepositoryType::Image(), role, targets_json, signer);

    // Only compare targets version in snapshot metadata for top-level
    // targets.json. Delegated target metadata versions are not tracked outside
    // of their own metadata.
    if (role == Uptane::Role::Targets() && targets[role.ToString()].version() != snapshot.targets_version()) {
      return false;
    }
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for images targets metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

std::unique_ptr<Uptane::Target> ImagesRepository::getTarget(const Uptane::Target& director_target) {
  std::unique_ptr<Uptane::Target> rv;
  auto cb = [director_target, &rv](const Uptane::Target& t) {
    if (director_target == t){
      rv = std_::make_unique<Target>(t);
      return false;
    }
    return true;
  };
  iterateTargets(cb);
  return rv;
}

void ImagesRepository::iterateTargets(std::function<bool(const Uptane::Target& t)> callback) {
  // Search for the target in the top-level targets metadata.
  Uptane::Targets toplevel = targets["targets"];
  for (const auto& it : toplevel.targets) {
    if (!callback(it)) return;
  }

  for (const auto& delegate_name : toplevel.delegated_role_names_) {
    Role delegate_role = Role::Delegation(delegate_name);
    Uptane::Targets delegate = targets[delegate_name];
    for (const auto& it : delegate.targets) {
      if (!callback(it)) return;
    }
    // TODO: recurse here if the delegation is non-terminating. For now,
    // assume that all delegations are terminating.
    if (!toplevel.terminating_role_[delegate_role]) {
      LOG_ERROR << "Nested delegations are not currently supported.";
    }
  }
}

}  // namespace Uptane
