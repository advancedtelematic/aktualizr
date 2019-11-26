#ifndef AKTUALIZR_SECONDARY_METADATA_H_
#define AKTUALIZR_SECONDARY_METADATA_H_

#include <unordered_map>

#include "uptane/fetcher.h"

class Metadata : public Uptane::IMetadataFetcher {
 public:
  Metadata(const Uptane::RawMetaPack& meta_pack);

  bool fetchRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo, const Uptane::Role& role,
                 Uptane::Version version) const override;
  bool fetchLatestRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo,
                       const Uptane::Role& role) const override;

 private:
  bool getRoleMetadata(std::string* result, const Uptane::RepositoryType& repo, const Uptane::Role& role) const;

 private:
  const std::unordered_map<std::string, std::string> _director_metadata;
  const std::unordered_map<std::string, std::string> _image_metadata;
};

#endif  // AKTUALIZR_SECONDARY_METADATA_H_
