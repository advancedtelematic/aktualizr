#ifndef AKTUALIZR_SECONDARY_METADATA_H_
#define AKTUALIZR_SECONDARY_METADATA_H_

#include "uptane/fetcher.h"
#include "uptane/tuf.h"

class Metadata : public Uptane::IMetadataFetcher {
 public:
  Metadata(Uptane::MetaBundle meta_bundle_in);
  Metadata(Metadata&&) = default;

  void fetchRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo, const Uptane::Role& role,
                 Uptane::Version version) const override;
  void fetchLatestRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo,
                       const Uptane::Role& role) const override;

 protected:
  virtual void getRoleMetadata(std::string* result, const Uptane::RepositoryType& repo, const Uptane::Role& role,
                               Uptane::Version version) const;

 private:
  const Uptane::MetaBundle meta_bundle_;
  Uptane::Version director_root_version_;
  Uptane::Version image_root_version_;
};

#endif  // AKTUALIZR_SECONDARY_METADATA_H_
