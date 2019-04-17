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
  bool targetsExpired() const { return latest_targets.isExpired(TimeStamp::Now()); }
  // Don't store the new targets if they are empty and we've previously received
  // a non-empty list.
  bool usePreviousTargets() const { return !targets.targets.empty() && latest_targets.targets.empty(); }
  bool checkMetaOffline(INvStorage& storage);

  Exception getLastException() const { return last_exception; }
  bool updateMeta(INvStorage& storage, Fetcher& fetcher);

 private:
  // Since the Director can send us an empty targets list to mean "no new
  // updates", we have to persist the previous targets list. Use the latest for
  // checking expiration but the most recent non-empty list for everything else.
  Uptane::Targets targets;         // Only empty if we've never received non-empty targets.
  Uptane::Targets latest_targets;  // Can be an empty list.
  Exception last_exception{"", ""};
};

}  // namespace Uptane

#endif  // DIRECTOR_REPOSITORY_H
