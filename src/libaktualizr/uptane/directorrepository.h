#ifndef DIRECTOR_REPOSITORY_H_
#define DIRECTOR_REPOSITORY_H_

#include "fetcher.h"
#include "uptanerepository.h"

namespace Uptane {

/* Director repository encapsulates state of metadata verification process. Subsequent verificaton steps rely on
 * previous ones.
 */
class DirectorRepository : public RepositoryCommon {
 public:
  DirectorRepository() : RepositoryCommon(RepositoryType::Director()) {}
  void resetMeta();

  bool verifyTargets(const std::string& targets_raw);
  const std::vector<Target>& getTargets() const { return targets.targets; }
  const std::string& getCorrelationId() const { return targets.correlation_id(); }
  bool targetsExpired() const { return targets.isExpired(TimeStamp::Now()); }
  bool checkMetaOffline(INvStorage& storage);

  Exception getLastException() const { return last_exception; }
  bool updateMeta(INvStorage& storage, Fetcher& fetcher);

 private:
  Uptane::Targets targets;
  Exception last_exception{"", ""};
};

}  // namespace Uptane

#endif  // DIRECTOR_REPOSITORY_H
