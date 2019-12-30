#ifndef IMAGES_REPOSITORY_H_
#define IMAGES_REPOSITORY_H_

#include "uptanerepository.h"

namespace Uptane {

constexpr int kDelegationsMaxDepth = 5;

class ImagesRepository : public RepositoryCommon {
 public:
  ImagesRepository() : RepositoryCommon(RepositoryType::Image()) {}

  Exception getLastException() const { return last_exception; }
  std::shared_ptr<const Uptane::Targets> getTargets() const { return targets; }

  bool verifyRoleHashes(const std::string& role_data, const Uptane::Role& role) const;
  int getRoleVersion(const Uptane::Role& role) const;
  int64_t getRoleSize(const Uptane::Role& role) const;
  static std::shared_ptr<Uptane::Targets> verifyDelegation(const std::string& delegation_raw, const Uptane::Role& role,
                                                           const Targets& parent_target);
  bool updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) override;
  bool checkMetaOffline(INvStorage& storage);

 private:
  bool targetsExpired() { return targets->isExpired(TimeStamp::Now()); }
  bool timestampExpired() { return timestamp.isExpired(TimeStamp::Now()); }
  bool snapshotExpired() { return snapshot.isExpired(TimeStamp::Now()); }
  int64_t snapshotSize() { return timestamp.snapshot_size(); }

  void resetMeta();
  bool verifyTimestamp(const std::string& timestamp_raw);
  bool fetchSnapshot(INvStorage& storage, const IMetadataFetcher& fetcher, int local_version);
  bool verifySnapshot(const std::string& snapshot_raw);
  bool fetchTargets(INvStorage& storage, const IMetadataFetcher& fetcher, int local_version);
  bool verifyTargets(const std::string& targets_raw);

  std::shared_ptr<Uptane::Targets> targets;
  Uptane::TimestampMeta timestamp;
  Uptane::Snapshot snapshot;

  Exception last_exception{"", ""};
};

}  // namespace Uptane

#endif  // IMAGES_REPOSITORY_H
