#ifndef AKTUALIZR_SECONDARY_METADATA_H_
#define AKTUALIZR_SECONDARY_METADATA_H_

#include <unordered_map>

#include "uptane/fetcher.h"

class Metadata : public Uptane::IMetadataFetcher {
 public:
  Metadata(const Uptane::RawMetaPack& meta_pack);
  Metadata(Metadata&&) = default;

  void fetchRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo, const Uptane::Role& role,
                 Uptane::Version version) const override;
  void fetchLatestRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo,
                       const Uptane::Role& role) const override;

 protected:
  virtual void getRoleMetadata(std::string* result, const Uptane::RepositoryType& repo, const Uptane::Role& role,
                               Uptane::Version version) const;

 private:
  const std::unordered_map<std::string, std::string> director_metadata_;
  const std::unordered_map<std::string, std::string> image_metadata_;
  Uptane::Version director_root_version_;
  Uptane::Version image_root_version_;
};

#endif  // AKTUALIZR_SECONDARY_METADATA_H_
