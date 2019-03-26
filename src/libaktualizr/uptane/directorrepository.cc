#include "directorrepository.h"

namespace Uptane {

void DirectorRepository::resetMeta() {
  resetRoot();
  targets = Targets();
}

bool DirectorRepository::verifyTargets(const std::string& targets_raw) {
  try {
    // Verify the signature:
    targets = Targets(RepositoryType::Director(), Role::Targets(), Utils::parseJSON(targets_raw),
                      std::make_shared<MetaWithKeys>(root));
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

}  // namespace Uptane
