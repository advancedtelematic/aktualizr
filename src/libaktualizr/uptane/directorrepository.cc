#include "directorrepository.h"

namespace Uptane {

void DirectorRepository::resetMeta() {
  resetRoot();
  targets = Targets();
  latest_targets = Targets();
}

void DirectorRepository::checkTargetsExpired() {
  if (latest_targets.isExpired(TimeStamp::Now())) {
    throw Uptane::ExpiredMetadata(type.toString(), Role::TARGETS);
  }
}

void DirectorRepository::targetsSanityCheck() {
  //  5.4.4.6.6. If checking Targets metadata from the Director repository,
  //  verify that there are no delegations.
  if (!latest_targets.delegated_role_names_.empty()) {
    throw Uptane::InvalidMetadata(type.toString(), Role::TARGETS, "Found unexpected delegation.");
  }
  //  5.4.4.6.7. If checking Targets metadata from the Director repository,
  //  check that no ECU identifier is represented more than once.
  std::set<Uptane::EcuSerial> ecu_ids;
  for (const auto& target : targets.targets) {
    for (const auto& ecu : target.ecus()) {
      if (ecu_ids.find(ecu.first) == ecu_ids.end()) {
        ecu_ids.insert(ecu.first);
      } else {
        LOG_ERROR << "ECU " << ecu.first << " appears twice in Director's Targets";
        throw Uptane::InvalidMetadata(type.toString(), Role::TARGETS, "Found repeated ECU ID.");
      }
    }
  }
}

bool DirectorRepository::usePreviousTargets() const {
  // Don't store the new targets if they are empty and we've previously received
  // a non-empty list.
  return !targets.targets.empty() && latest_targets.targets.empty();
}

void DirectorRepository::verifyTargets(const std::string& targets_raw) {
  try {
    // Verify the signature:
    latest_targets = Targets(RepositoryType::Director(), Role::Targets(), Utils::parseJSON(targets_raw),
                             std::make_shared<MetaWithKeys>(root));
    if (!usePreviousTargets()) {
      targets = latest_targets;
    }
  } catch (const Uptane::Exception& e) {
    LOG_ERROR << "Signature verification for Director Targets metadata failed";
    throw;
  }
}

void DirectorRepository::checkMetaOffline(INvStorage& storage) {
  resetMeta();
  // Load Director Root Metadata
  {
    std::string director_root;
    if (!storage.loadLatestRoot(&director_root, RepositoryType::Director())) {
      throw Uptane::SecurityException(RepositoryType::DIRECTOR, "Could not load latest root");
    }

    initRoot(RepositoryType(RepositoryType::DIRECTOR), director_root);

    if (rootExpired()) {
      throw Uptane::ExpiredMetadata(RepositoryType::DIRECTOR, Role::ROOT);
    }
  }

  // Load Director Targets Metadata
  {
    std::string director_targets;

    if (!storage.loadNonRoot(&director_targets, RepositoryType::Director(), Role::Targets())) {
      throw Uptane::SecurityException(RepositoryType::DIRECTOR, "Could not load Targets role");
    }

    verifyTargets(director_targets);

    checkTargetsExpired();

    targetsSanityCheck();
  }
}

void DirectorRepository::updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) {
  // Uptane step 2 (download time) is not implemented yet.
  // Uptane step 3 (download metadata)

  // reset Director repo to initial state before starting Uptane iteration
  resetMeta();

  updateRoot(storage, fetcher, RepositoryType::Director());

  // Not supported: 3. Download and check the Timestamp metadata file from the Director repository, following the
  // procedure in Section 5.4.4.4. Not supported: 4. Download and check the Snapshot metadata file from the Director
  // repository, following the procedure in Section 5.4.4.5.

  // Update Director Targets Metadata
  {
    std::string director_targets;

    fetcher.fetchLatestRole(&director_targets, kMaxDirectorTargetsSize, RepositoryType::Director(), Role::Targets());
    int remote_version = extractVersionUntrusted(director_targets);

    int local_version;
    std::string director_targets_stored;
    if (storage.loadNonRoot(&director_targets_stored, RepositoryType::Director(), Role::Targets())) {
      local_version = extractVersionUntrusted(director_targets_stored);
      try {
        verifyTargets(director_targets_stored);
      } catch (const std::exception& e) {
        LOG_WARNING << "Unable to verify stored Director Targets metadata.";
      }
    } else {
      local_version = -1;
    }

    verifyTargets(director_targets);

    // TODO(OTA-4940): check if versions are equal but content is different. In
    // that case, the member variable targets is updated, but it isn't stored in
    // the database, which can cause some minor confusion.
    if (local_version > remote_version) {
      throw Uptane::SecurityException(RepositoryType::DIRECTOR, "Rollback attempt");
    } else if (local_version < remote_version && !usePreviousTargets()) {
      storage.storeNonRoot(director_targets, RepositoryType::Director(), Role::Targets());
    }

    checkTargetsExpired();

    targetsSanityCheck();
  }
}

void DirectorRepository::dropTargets(INvStorage& storage) {
  try {
    storage.clearNonRootMeta(RepositoryType::Director());
    resetMeta();
  } catch (const Uptane::Exception& ex) {
    LOG_ERROR << "Failed to reset Director Targets metadata: " << ex.what();
  }
}

bool DirectorRepository::matchTargetsWithImageTargets(const Uptane::Targets& image_targets) const {
  // step 10 of https://uptane.github.io/papers/ieee-isto-6100.1.0.0.uptane-standard.html#rfc.section.5.4.4.2
  // TODO(OTA-4800): support delegations. Consider reusing findTargetInDelegationTree(),
  // but it would need to be moved into a common place to be resued by Primary and Secondary.
  // Currently this is only used by aktualizr-secondary, but according to the
  // Standard, "A Secondary ECU MAY elect to perform this check only on the
  // metadata for the image it will install".
  const auto& image_target_array = image_targets.targets;
  const auto& director_target_array = targets.targets;

  for (const auto& director_target : director_target_array) {
    auto found_it = std::find_if(
        image_target_array.begin(), image_target_array.end(),
        [&director_target](const Target& image_target) { return director_target.MatchTarget(image_target); });

    if (found_it == image_target_array.end()) {
      return false;
    }
  }

  return true;
}

}  // namespace Uptane
