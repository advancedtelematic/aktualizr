#ifndef IMAGES_REPOSITORY_H_
#define IMAGES_REPOSITORY_H_

#include "uptanerepository.h"

namespace Uptane {

class ImagesRepository : public RepositoryCommon {
 public:
  ImagesRepository() : RepositoryCommon(RepositoryType::Image()) {}

  void resetMeta();

  bool verifyTargets(const std::string& targets_raw);
  bool targetsExpired() { return targets.isExpired(TimeStamp::Now()); }
  int64_t targetsSize() { return snapshot.targets_size(); }
  std::unique_ptr<Uptane::Target> getTarget(const Uptane::Target& director_target);

  bool verifyTimestamp(const std::string& timestamp_raw);
  bool timestampExpired() { return timestamp.isExpired(TimeStamp::Now()); }

  bool verifySnapshot(const std::string& snapshot_raw);
  bool snapshotExpired() { return snapshot.isExpired(TimeStamp::Now()); }
  int64_t snapshotSize() { return timestamp.snapshot_size(); }

  const std::vector<Target> getTargets() { return targets.targets; }

  Exception getLastException() const { return last_exception; }

 private:
  Uptane::Targets targets;
  Uptane::TimestampMeta timestamp;
  Uptane::Snapshot snapshot;

  Exception last_exception{"", ""};
};

}  // namespace Uptane

#endif  // IMAGES_REPOSITORY_H
