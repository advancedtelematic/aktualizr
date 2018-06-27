#include "directorrepository.h"

namespace Uptane {

void DirectorRepository::resetMeta() {
  resetRoot();
  targets = Targets();
}

bool DirectorRepository::verifyTargets(const std::string& targets_raw) {
  try {
    targets = Targets(RepositoryType::Director, Utils::parseJSON(targets_raw), root);  // signature verification
  } catch (Uptane::Exception e) {
    LOG_ERROR << "Signature verification for director targets metadata failed";
    last_exception = e;
    return false;
  }
  return true;
}

}  // namespace Uptane
