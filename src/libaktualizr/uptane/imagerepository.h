#ifndef IMAGE_REPOSITORY_H_
#define IMAGE_REPOSITORY_H_

#include <map>
#include <vector>

#include "uptanerepository.h"

namespace Uptane {

constexpr int kDelegationsMaxDepth = 5;

class ImageRepository : public RepositoryCommon {
 public:
  ImageRepository() : RepositoryCommon(RepositoryType::Image()) {}

  void resetMeta();

  void verifyTargets(const std::string& targets_raw, bool prefetch);

  void verifyTimestamp(const std::string& timestamp_raw);

  void verifySnapshot(const std::string& snapshot_raw, bool prefetch);

  static std::shared_ptr<Uptane::Targets> verifyDelegation(const std::string& delegation_raw, const Uptane::Role& role,
                                                           const Targets& parent_target);
  std::shared_ptr<const Uptane::Targets> getTargets() const { return targets; }

  void verifyRoleHashes(const std::string& role_data, const Uptane::Role& role, bool prefetch) const;
  int getRoleVersion(const Uptane::Role& role) const;
  int64_t getRoleSize(const Uptane::Role& role) const;

  void checkMetaOffline(INvStorage& storage);
  void updateMeta(INvStorage& storage, const IMetadataFetcher& fetcher) override;

 private:
  void checkTimestampExpired();
  void checkSnapshotExpired();
  int64_t snapshotSize() const { return timestamp.snapshot_size(); }
  void fetchSnapshot(INvStorage& storage, const IMetadataFetcher& fetcher, int local_version);
  void fetchTargets(INvStorage& storage, const IMetadataFetcher& fetcher, int local_version);
  void checkTargetsExpired();

  std::shared_ptr<Uptane::Targets> targets;
  Uptane::TimestampMeta timestamp;
  Uptane::Snapshot snapshot;
};

}  // namespace Uptane

#endif  // IMAGE_REPOSITORY_H_
