#include "directorrepository.h"

namespace Uptane {

void DirectorRepository::resetMeta() {
  resetRoot();
  targets = Targets();
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

bool DirectorRepository::updateMeta(INvStorage& storage, Fetcher& fetcher) {
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
    std::string director_root;
    if (!fetcher.fetchLatestRole(&director_root, kMaxRootSize, RepositoryType::Director(), Role::Root())) {
      return false;
    }
    int remote_version = extractVersionUntrusted(director_root);
    int local_version = rootVersion();

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

    if (rootExpired()) {
      return false;
    }
  }
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

}  // namespace Uptane
