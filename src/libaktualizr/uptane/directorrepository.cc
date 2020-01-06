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

  if (!updateRoot(storage, fetcher, RepositoryType::Director())) {
    return false;
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
