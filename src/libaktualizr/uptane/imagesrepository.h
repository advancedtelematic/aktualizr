#ifndef IMAGES_REPOSITORY_H_
#define IMAGES_REPOSITORY_H_

#include <map>
#include <vector>

#include "uptanerepository.h"

namespace Uptane {

constexpr int kDelegationsMaxDepth = 5;

class ImagesRepository : public RepositoryCommon {
 public:
  ImagesRepository() : RepositoryCommon(RepositoryType::Image()) {}

  Exception getLastException() const { return last_exception; }

  static std::shared_ptr<Uptane::Targets> verifyDelegation(const std::string& delegation_raw, const Uptane::Role& role,
                                                           const Targets& parent_target);
  std::shared_ptr<const Uptane::Targets> getTargets() const { return targets; }

  bool verifyRoleHashes(const std::string& role_data, const Uptane::Role& role) const;
  int getRoleVersion(const Uptane::Role& role) const;
  int64_t getRoleSize(const Uptane::Role& role) const;

  bool checkMetaOffline(INvStorage& storage);
  bool updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) override;

 private:
  void resetMeta();
  bool verifyTargets(const std::string& targets_raw);
  bool targetsExpired() { return targets->isExpired(TimeStamp::Now()); }
  bool verifyTimestamp(const std::string& timestamp_raw);
  bool timestampExpired() { return timestamp.isExpired(TimeStamp::Now()); }
  bool verifySnapshot(const std::string& snapshot_raw);
  bool snapshotExpired() { return snapshot.isExpired(TimeStamp::Now()); }
  int64_t snapshotSize() { return timestamp.snapshot_size(); }

 private:
  std::shared_ptr<Uptane::Targets> targets;
  Uptane::TimestampMeta timestamp;
  Uptane::Snapshot snapshot;

  Exception last_exception{"", ""};
};

}  // namespace Uptane

#endif  // IMAGES_REPOSITORY_H
