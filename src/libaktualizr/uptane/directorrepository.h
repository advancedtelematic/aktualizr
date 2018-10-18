#ifndef DIRECTOR_REPOSITORY_H_
#define DIRECTOR_REPOSITORY_H_

#include "uptanerepository.h"

namespace Uptane {

/* Director repository encapsulates state of metadata verification process. Subsequent verificaton steps rely on
 * previous ones.
 */
class DirectorRepository : public RepositoryCommon {
 public:
  DirectorRepository() : RepositoryCommon(RepositoryType::Director) {}
  void resetMeta();

  bool verifyTargets(const std::string& targets_raw);
  const std::vector<Target>& getTargets() const { return targets.targets; }
  bool targetsExpired() const { return targets.isExpired(TimeStamp::Now()); }

  Exception getLastException() const { return last_exception; }

 private:
  Uptane::Targets targets;
  Exception last_exception{"", ""};
};

}  // namespace Uptane

#endif  // DIRECTOR_REPOSITORY_H
