#include "imagerepository.h"

namespace Uptane {

void ImageRepository::resetMeta() {
  resetRoot();
  targets.reset();
  snapshot = Snapshot();
  timestamp = TimestampMeta();
}

bool ImageRepository::verifyTimestamp(const std::string& timestamp_raw) {
  try {
    // Verify the signature:
    timestamp =
        TimestampMeta(RepositoryType::Image(), Utils::parseJSON(timestamp_raw), std::make_shared<MetaWithKeys>(root));
  } catch (const Exception& e) {
    LOG_ERROR << "Signature verification for Timestamp metadata failed";
    throw e;
  }
  return true;
}

bool ImageRepository::timestampExpired() {
  if (timestamp.isExpired(TimeStamp::Now())) {
    throw Uptane::ExpiredMetadata(type.toString(), Role::Timestamp().ToString());
  }
  return false;
}

bool ImageRepository::fetchSnapshot(INvStorage& storage, const IMetadataFetcher& fetcher, const int local_version) {
  std::string image_snapshot;
  const int64_t snapshot_size = (snapshotSize() > 0) ? snapshotSize() : kMaxSnapshotSize;
  if (!fetcher.fetchLatestRole(&image_snapshot, snapshot_size, RepositoryType::Image(), Role::Snapshot())) {
    return false;
  }
  const int remote_version = extractVersionUntrusted(image_snapshot);

  // 6. Check that each Targets metadata filename listed in the previous Snapshot metadata file is also listed in this
  // Snapshot metadata file. If this condition is not met, discard the new Snapshot metadata file, abort the update
  // cycle, and report the failure. (Checks for a rollback attack.)
  // See also https://github.com/uptane/deployment-considerations/pull/39/files.
  // If the Snapshot is rotated, delegations may be safely removed.
  // https://saeljira.it.here.com/browse/OTA-4121
  if (!verifySnapshot(image_snapshot, false)) {
    return false;
  }

  if (local_version > remote_version) {
    return false;
  } else if (local_version < remote_version) {
    storage.storeNonRoot(image_snapshot, RepositoryType::Image(), Role::Snapshot());
  }

  return true;
}

bool ImageRepository::verifySnapshot(const std::string& snapshot_raw, bool prefetch) {
  try {
    const std::string canonical = Utils::jsonToCanonicalStr(Utils::parseJSON(snapshot_raw));
    bool hash_exists = false;
    for (const auto& it : timestamp.snapshot_hashes()) {
      switch (it.type()) {
        case Hash::Type::kSha256:
          if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(canonical))) != it) {
            if (!prefetch) {
              LOG_ERROR << "Hash verification for Snapshot metadata failed";
            }
            return false;
          }
          hash_exists = true;
          break;
        case Hash::Type::kSha512:
          if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(canonical))) != it) {
            if (!prefetch) {
              LOG_ERROR << "Hash verification for Snapshot metadata failed";
            }
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
    throw e;
  }
  return true;
}

bool ImageRepository::snapshotExpired() {
  if (snapshot.isExpired(TimeStamp::Now())) {
    throw Uptane::ExpiredMetadata(type.toString(), Role::Snapshot().ToString());
  }
  return false;
}

bool ImageRepository::fetchTargets(INvStorage& storage, const IMetadataFetcher& fetcher, const int local_version) {
  std::string image_targets;
  const Role targets_role = Role::Targets();

  auto targets_size = getRoleSize(Role::Targets());
  if (targets_size <= 0) {
    targets_size = kMaxImageTargetsSize;
  }
  if (!fetcher.fetchLatestRole(&image_targets, targets_size, RepositoryType::Image(), targets_role)) {
    return false;
  }
  const int remote_version = extractVersionUntrusted(image_targets);

  if (!verifyTargets(image_targets, false)) {
    return false;
  }

  if (local_version > remote_version) {
    return false;
  } else if (local_version < remote_version) {
    storage.storeNonRoot(image_targets, RepositoryType::Image(), targets_role);
  }

  return true;
}

bool ImageRepository::verifyRoleHashes(const std::string& role_data, const Uptane::Role& role, bool prefetch) const {
  const std::string canonical = Utils::jsonToCanonicalStr(Utils::parseJSON(role_data));
  // Hashes are not required. If present, however, we may as well check them.
  // This provides no security benefit, but may help with fault detection.
  for (const auto& it : snapshot.role_hashes(role)) {
    switch (it.type()) {
      case Hash::Type::kSha256:
        if (Hash(Hash::Type::kSha256, boost::algorithm::hex(Crypto::sha256digest(canonical))) != it) {
          if (!prefetch) {
            LOG_ERROR << "Hash verification for " << role.ToString() << " metadata failed";
          }
          return false;
        }
        break;
      case Hash::Type::kSha512:
        if (Hash(Hash::Type::kSha512, boost::algorithm::hex(Crypto::sha512digest(canonical))) != it) {
          if (!prefetch) {
            LOG_ERROR << "Hash verification for " << role.ToString() << " metadata failed";
          }
          return false;
        }
        break;
      default:
        break;
    }
  }

  return true;
}

int ImageRepository::getRoleVersion(const Uptane::Role& role) const { return snapshot.role_version(role); }

int64_t ImageRepository::getRoleSize(const Uptane::Role& role) const { return snapshot.role_size(role); }

bool ImageRepository::verifyTargets(const std::string& targets_raw, bool prefetch) {
  try {
    if (!verifyRoleHashes(targets_raw, Uptane::Role::Targets(), prefetch)) {
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
    throw e;
  }
  return true;
}

std::shared_ptr<Uptane::Targets> ImageRepository::verifyDelegation(const std::string& delegation_raw,
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

bool ImageRepository::targetsExpired() {
  if (targets->isExpired(TimeStamp::Now())) {
    throw Uptane::ExpiredMetadata(type.toString(), Role::Targets().ToString());
  }
  return false;
}

void ImageRepository::updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) {
  resetMeta();

  if (!updateRoot(storage, fetcher, RepositoryType::Image())) {
    throw std::runtime_error("");
  }

  // Update Image repo Timestamp metadata
  {
    std::string image_timestamp;

    if (!fetcher.fetchLatestRole(&image_timestamp, kMaxTimestampSize, RepositoryType::Image(), Role::Timestamp())) {
      throw std::runtime_error("");
    }
    int remote_version = extractVersionUntrusted(image_timestamp);

    int local_version;
    std::string image_timestamp_stored;
    if (storage.loadNonRoot(&image_timestamp_stored, RepositoryType::Image(), Role::Timestamp())) {
      local_version = extractVersionUntrusted(image_timestamp_stored);
    } else {
      local_version = -1;
    }

    if (!verifyTimestamp(image_timestamp)) {
      throw std::runtime_error("");
    }

    if (local_version > remote_version) {
      throw std::runtime_error("");
    } else if (local_version < remote_version) {
      storage.storeNonRoot(image_timestamp, RepositoryType::Image(), Role::Timestamp());
    }

    if (timestampExpired()) {
      throw std::runtime_error("");
    }
  }

  // Update Image repo Snapshot metadata
  {
    // First check if we already have the latest version according to the
    // Timestamp metadata.
    bool fetch_snapshot = true;
    int local_version;
    std::string image_snapshot_stored;
    if (storage.loadNonRoot(&image_snapshot_stored, RepositoryType::Image(), Role::Snapshot())) {
      if (verifySnapshot(image_snapshot_stored, true)) {
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
        throw std::runtime_error("");
      }
    }

    if (snapshotExpired()) {
      throw std::runtime_error("");
    }
  }

  // Update Image repo Targets metadata
  {
    // First check if we already have the latest version according to the
    // Snapshot metadata.
    bool fetch_targets = true;
    int local_version = -1;
    std::string image_targets_stored;
    if (storage.loadNonRoot(&image_targets_stored, RepositoryType::Image(), Role::Targets())) {
      if (verifyTargets(image_targets_stored, true)) {
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
        throw std::runtime_error("");
      }
    }

    if (targetsExpired()) {
      throw std::runtime_error("");
    }
  }
}

void ImageRepository::checkMetaOffline(INvStorage& storage) {
  resetMeta();
  // Load Image repo Root metadata
  {
    std::string image_root;
    if (!storage.loadLatestRoot(&image_root, RepositoryType::Image())) {
      throw std::runtime_error("");
    }

    if (!initRoot(image_root)) {
      throw std::runtime_error("");
    }

    if (rootExpired()) {
      throw std::runtime_error("");
    }
  }

  // Load Image repo Timestamp metadata
  {
    std::string image_timestamp;
    if (!storage.loadNonRoot(&image_timestamp, RepositoryType::Image(), Role::Timestamp())) {
      throw std::runtime_error("");
    }

    if (!verifyTimestamp(image_timestamp)) {
      throw std::runtime_error("");
    }

    if (timestampExpired()) {
      throw std::runtime_error("");
    }
  }

  // Load Image repo Snapshot metadata
  {
    std::string image_snapshot;

    if (!storage.loadNonRoot(&image_snapshot, RepositoryType::Image(), Role::Snapshot())) {
      throw std::runtime_error("");
    }

    if (!verifySnapshot(image_snapshot, false)) {
      throw std::runtime_error("");
    }

    if (snapshotExpired()) {
      throw std::runtime_error("");
    }
  }

  // Load Image repo Targets metadata
  {
    std::string image_targets;
    Role targets_role = Uptane::Role::Targets();
    if (!storage.loadNonRoot(&image_targets, RepositoryType::Image(), targets_role)) {
      throw std::runtime_error("");
    }

    if (!verifyTargets(image_targets, false)) {
      throw std::runtime_error("");
    }

    if (targetsExpired()) {
      throw std::runtime_error("");
    }
  }
}

}  // namespace Uptane
