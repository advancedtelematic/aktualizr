#ifndef IMAGES_REPOSITORY_H_
#define IMAGES_REPOSITORY_H_

#include <map>
#include <vector>

#include "uptanerepository.h"

namespace Uptane {

class ImagesRepository : public RepositoryCommon {
 public:
  ImagesRepository() : RepositoryCommon(RepositoryType::Image()) {}

  void resetMeta();

  bool verifyTargets(const std::string& targets_raw, Uptane::Role role);
  bool targetsExpired(const std::string& role_name) { return targets[role_name].isExpired(TimeStamp::Now()); }
  int64_t targetsSize() { return snapshot.targets_size(); }
  std::set<std::string> delegations(const std::string& role_name) { return targets[role_name].delegated_role_names_; }
  std::unique_ptr<Uptane::Target> getTarget(const Uptane::Target& director_target);

  bool verifyTimestamp(const std::string& timestamp_raw);
  bool timestampExpired() { return timestamp.isExpired(TimeStamp::Now()); }

  bool verifySnapshot(const std::string& snapshot_raw);
  bool snapshotExpired() { return snapshot.isExpired(TimeStamp::Now()); }
  int64_t snapshotSize() { return timestamp.snapshot_size(); }

  Exception getLastException() const { return last_exception; }

 private:
  // Map from role name -> metadata ("targets" for top-level):
  std::map<std::string, Uptane::Targets> targets;
  Uptane::TimestampMeta timestamp;
  Uptane::Snapshot snapshot;

  Exception last_exception{"", ""};
};

}  // namespace Uptane

#endif  // IMAGES_REPOSITORY_H
