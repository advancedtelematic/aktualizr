#include "directorrepository.h"

namespace Uptane {

void DirectorRepository::resetMeta() {
  resetRoot();
  targets = Targets();
  latest_targets = Targets();
}

bool DirectorRepository::targetsExpired() const { return latest_targets.isExpired(TimeStamp::Now()); }

bool DirectorRepository::usePreviousTargets() const {
  // Don't store the new targets if they are empty and we've previously received
  // a non-empty list.
  return !targets.targets.empty() && latest_targets.targets.empty();
}

bool DirectorRepository::verifyTargets(const std::string& targets_raw) {
  try {
    // Verify the signature:
    latest_targets = Targets(RepositoryType::Director(), Role::Targets(), Utils::parseJSON(targets_raw),
                             std::make_shared<MetaWithKeys>(root));
    if (!usePreviousTargets()) {
      targets = latest_targets;
    }
  } catch (const Uptane::Exception& e) {
    LOG_ERROR << "Signature verification for director targets metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

bool DirectorRepository::checkMetaOffline(INvStorage& storage) {
  resetMeta();
  // Load Director Root Metadata
  {
    std::string director_root;
    if (!storage.loadLatestRoot(&director_root, RepositoryType::Director())) {
      return false;
    }

    if (!initRoot(director_root)) {
      return false;
    }

    if (rootExpired()) {
      return false;
    }
  }

  // Load Director Targets Metadata
  {
    std::string director_targets;

    if (!storage.loadNonRoot(&director_targets, RepositoryType::Director(), Role::Targets())) {
      return false;
    }

    if (!verifyTargets(director_targets)) {
      return false;
    }

    if (targetsExpired()) {
      return false;
    }
  }

  return true;
}

bool DirectorRepository::updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) {
  // Uptane step 2 (download time) is not implemented yet.
  // Uptane step 3 (download metadata)

  // reset director repo to initial state before starting UPTANE iteration
  resetMeta();
  // Load Initial Director Root Metadata
  {
    std::string director_root;
    if (storage.loadLatestRoot(&director_root, RepositoryType::Director())) {
      if (!initRoot(director_root)) {
        return false;
      }
    } else {
      if (!fetcher.fetchRole(&director_root, kMaxRootSize, RepositoryType::Director(), Role::Root(), Version(1))) {
        return false;
      }
      if (!initRoot(director_root)) {
        return false;
      }
      storage.storeRoot(director_root, RepositoryType::Director(), Version(1));
    }
  }

  // Update Director Root Metadata
  {
    // According to the current design root.json without a number is guaranteed to be the latest version.
    // Therefore we fetch the latest (root.json), and
    // if it matches what we already have stored, we're good.
    // If not, then we have to go fetch the missing ones by name/number until we catch up.

    std::string director_root;
    if (!fetcher.fetchLatestRole(&director_root, kMaxRootSize, RepositoryType::Director(), Role::Root())) {
      return false;
    }
    int remote_version = extractVersionUntrusted(director_root);
    if (remote_version == -1) {
      LOG_ERROR << "Failed to extract a version from Director's root.json: " << director_root;
      return false;
    }

    int local_version = rootVersion();

    // if remote_version <= local_version then the director's root metadata are never verified
    // which leads to two issues
    // 1. At initial stage (just after provisioning) the root metadata from 1.root.json are not verified
    // 2. If local_version becomes higher than 1, e.g. 2 than a rollback attack is possible since the business logic
    // here won't return any error as suggested in #4 of
    // https://uptane.github.io/uptane-standard/uptane-standard.html#check_root
    // TODO: https://saeljira.it.here.com/browse/OTA-4119
    for (int version = local_version + 1; version <= remote_version; ++version) {
      if (!fetcher.fetchRole(&director_root, kMaxRootSize, RepositoryType::Director(), Role::Root(),
                             Version(version))) {
        return false;
      }

      if (!verifyRoot(director_root)) {
        return false;
      }
      storage.storeRoot(director_root, RepositoryType::Director(), Version(version));
      storage.clearNonRootMeta(RepositoryType::Director());
    }

    // Check that the current (or latest securely attested) time is lower than the expiration timestamp in the latest
    // Root metadata file. (Checks for a freeze attack.)
    if (rootExpired()) {
      return false;
    }
  }

  // Not supported: 3. Download and check the Timestamp metadata file from the Director repository, following the
  // procedure in Section 5.4.4.4. Not supported: 4. Download and check the Snapshot metadata file from the Director
  // repository, following the procedure in Section 5.4.4.5.

  // Update Director Targets Metadata
  {
    std::string director_targets;

    if (!fetcher.fetchLatestRole(&director_targets, kMaxDirectorTargetsSize, RepositoryType::Director(),
                                 Role::Targets())) {
      return false;
    }
    int remote_version = extractVersionUntrusted(director_targets);

    int local_version;
    std::string director_targets_stored;
    if (storage.loadNonRoot(&director_targets_stored, RepositoryType::Director(), Role::Targets())) {
      local_version = extractVersionUntrusted(director_targets_stored);
      if (!verifyTargets(director_targets_stored)) {
        LOG_WARNING << "Unable to verify stored director targets metadata.";
      }
    } else {
      local_version = -1;
    }

    // Not supported: If the ECU performing the verification is the Primary ECU, it SHOULD also ensure that the Targets
    // metadata from the Director repository doesn’t contain any ECU identifiers for ECUs not actually present in the
    // vehicle.

    // Not supported: The followin steps of the Director's target metadata verification are missing in
    // DirectorRepository::updateMeta()
    //  6. If checking Targets metadata from the Director repository, verify that there are no delegations.
    //  7. If checking Targets metadata from the Director repository, check that no ECU identifier is represented more
    //  than once.
    // TODO: https://saeljira.it.here.com/browse/OTA-4118
    if (!verifyTargets(director_targets)) {
      return false;
    }

    if (local_version > remote_version) {
      return false;
    } else if (local_version < remote_version && !usePreviousTargets()) {
      storage.storeNonRoot(director_targets, RepositoryType::Director(), Role::Targets());
    }

    if (targetsExpired()) {
      return false;
    }
  }

  return true;
}

void DirectorRepository::dropTargets(INvStorage& storage) {
  storage.clearNonRootMeta(RepositoryType::Director());
  resetMeta();
}

}  // namespace Uptane
