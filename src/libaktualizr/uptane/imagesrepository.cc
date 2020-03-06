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
    LOG_ERROR << "Signature verification for Timestamp metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

bool ImagesRepository::fetchSnapshot(INvStorage& storage, const IMetadataFetcher& fetcher, const int local_version) {
  std::string images_snapshot;
  const int64_t snapshot_size = (snapshotSize() > 0) ? snapshotSize() : kMaxSnapshotSize;
  if (!fetcher.fetchLatestRole(&images_snapshot, snapshot_size, RepositoryType::Image(), Role::Snapshot())) {
    return false;
  }
  const int remote_version = extractVersionUntrusted(images_snapshot);

  // 6. Check that each Targets metadata filename listed in the previous Snapshot metadata file is also listed in this
  // Snapshot metadata file. If this condition is not met, discard the new Snapshot metadata file, abort the update
  // cycle, and report the failure. (Checks for a rollback attack.)
  // Let's wait for this ticket resolution https://github.com/uptane/uptane-standard/issues/149
  // https://saeljira.it.here.com/browse/OTA-4121
  if (!verifySnapshot(images_snapshot)) {
    return false;
  }

  if (local_version > remote_version) {
    return false;
  } else if (local_version < remote_version) {
    storage.storeNonRoot(images_snapshot, RepositoryType::Image(), Role::Snapshot());
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
            LOG_ERROR << "Hash verification for Snapshot metadata failed";
            return false;
          }
          hash_exists = true;
          break;
        case Hash::Type::kSha512:
          if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(canonical))) != it) {
            LOG_ERROR << "Hash verification for Snapshot metadata failed";
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
    LOG_ERROR << "Signature verification for Snapshot metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

bool ImagesRepository::fetchTargets(INvStorage& storage, const IMetadataFetcher& fetcher, const int local_version) {
  std::string images_targets;
  const Role targets_role = Role::Targets();

  auto targets_size = getRoleSize(Role::Targets());
  if (targets_size <= 0) {
    targets_size = kMaxImagesTargetsSize;
  }
  if (!fetcher.fetchLatestRole(&images_targets, targets_size, RepositoryType::Image(), targets_role)) {
    return false;
  }
  const int remote_version = extractVersionUntrusted(images_targets);

  if (!verifyTargets(images_targets)) {
    return false;
  }

  if (local_version > remote_version) {
    return false;
  } else if (local_version < remote_version) {
    storage.storeNonRoot(images_targets, RepositoryType::Image(), targets_role);
  }

  return true;
}

bool ImagesRepository::verifyRoleHashes(const std::string& role_data, const Uptane::Role& role) const {
  const std::string canonical = Utils::jsonToCanonicalStr(Utils::parseJSON(role_data));
  // Hashes are not required. If present, however, we may as well check them.
  // This provides no security benefit, but may help with fault detection.
  for (const auto& it : snapshot.role_hashes(role)) {
    switch (it.type()) {
      case Hash::Type::kSha256:
        if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(canonical))) != it) {
          LOG_ERROR << "Hash verification for " << role.ToString() << " metadata failed";
          return false;
        }
        break;
      case Hash::Type::kSha512:
        if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(canonical))) != it) {
          LOG_ERROR << "Hash verification for " << role.ToString() << " metadata failed";
          return false;
        }
        break;
      default:
        break;
    }
  }

  return true;
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
    LOG_ERROR << "Signature verification for Image repo Targets metadata failed";
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
    LOG_ERROR << "Signature verification for Image repo delegated Targets metadata failed";
    throw e;
  }

  return std::shared_ptr<Uptane::Targets>(nullptr);
}

bool ImagesRepository::updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) {
  resetMeta();

  if (!updateRoot(storage, fetcher, RepositoryType::Image())) {
    return false;
  }

  // Update Images Timestamp Metadata
  {
    std::string images_timestamp;

    if (!fetcher.fetchLatestRole(&images_timestamp, kMaxTimestampSize, RepositoryType::Image(), Role::Timestamp())) {
      return false;
    }
    int remote_version = extractVersionUntrusted(images_timestamp);

    int local_version;
    std::string images_timestamp_stored;
    if (storage.loadNonRoot(&images_timestamp_stored, RepositoryType::Image(), Role::Timestamp())) {
      local_version = extractVersionUntrusted(images_timestamp_stored);
    } else {
      local_version = -1;
    }

    if (!verifyTimestamp(images_timestamp)) {
      return false;
    }

    if (local_version > remote_version) {
      return false;
    } else if (local_version < remote_version) {
      storage.storeNonRoot(images_timestamp, RepositoryType::Image(), Role::Timestamp());
    }

    if (timestampExpired()) {
      return false;
    }
  }

  // Update Images Snapshot Metadata
  {
    // First check if we already have the latest version according to the
    // Timestamp metadata.
    bool fetch_snapshot = true;
    int local_version;
    std::string images_snapshot_stored;
    if (storage.loadNonRoot(&images_snapshot_stored, RepositoryType::Image(), Role::Snapshot())) {
      if (verifySnapshot(images_snapshot_stored)) {
        fetch_snapshot = false;
        LOG_DEBUG << "Skipping Image repo Snapshot download; stored version is still current.";
      }
      local_version = snapshot.version();
    } else {
      local_version = -1;
    }

    // If we don't, attempt to fetch the latest.
    if (fetch_snapshot) {
      if (!fetchSnapshot(storage, fetcher, local_version)) {
        return false;
      }
    }

    if (snapshotExpired()) {
      return false;
    }
  }

  // Update Images Targets Metadata
  {
    // First check if we already have the latest version according to the
    // Snapshot metadata.
    bool fetch_targets = true;
    int local_version = -1;
    std::string images_targets_stored;
    if (storage.loadNonRoot(&images_targets_stored, RepositoryType::Image(), Role::Targets())) {
      if (verifyTargets(images_targets_stored)) {
        fetch_targets = false;
        LOG_DEBUG << "Skipping Image repo Targets download; stored version is still current.";
      }
      if (targets) {
        local_version = targets->version();
      }
    }

    // If we don't, attempt to fetch the latest.
    if (fetch_targets) {
      if (!fetchTargets(storage, fetcher, local_version)) {
        return false;
      }
    }

    if (targetsExpired()) {
      return false;
    }
  }

  return true;
}

bool ImagesRepository::checkMetaOffline(INvStorage& storage) {
  resetMeta();
  // Load Images Root Metadata
  {
    std::string images_root;
    if (!storage.loadLatestRoot(&images_root, RepositoryType::Image())) {
      return false;
    }

    if (!initRoot(images_root)) {
      return false;
    }

    if (rootExpired()) {
      return false;
    }
  }

  // Load Images Timestamp Metadata
  {
    std::string images_timestamp;
    if (!storage.loadNonRoot(&images_timestamp, RepositoryType::Image(), Role::Timestamp())) {
      return false;
    }

    if (!verifyTimestamp(images_timestamp)) {
      return false;
    }

    if (timestampExpired()) {
      return false;
    }
  }

  // Load Images Snapshot Metadata
  {
    std::string images_snapshot;

    if (!storage.loadNonRoot(&images_snapshot, RepositoryType::Image(), Role::Snapshot())) {
      return false;
    }

    if (!verifySnapshot(images_snapshot)) {
      return false;
    }

    if (snapshotExpired()) {
      return false;
    }
  }

  // Load Images Targets Metadata
  {
    std::string images_targets;
    Role targets_role = Uptane::Role::Targets();
    if (!storage.loadNonRoot(&images_targets, RepositoryType::Image(), targets_role)) {
      return false;
    }

    if (!verifyTargets(images_targets)) {
      return false;
    }

    if (targetsExpired()) {
      return false;
    }
  }

  return true;
}

}  // namespace Uptane
