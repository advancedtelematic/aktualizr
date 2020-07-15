#include "aktualizr_secondary_metadata.h"

Metadata::Metadata(Uptane::MetaBundle meta_bundle_in) : meta_bundle_(std::move(meta_bundle_in)) {
  director_root_version_ = Uptane::Version(Uptane::extractVersionUntrusted(
      Uptane::getMetaFromBundle(meta_bundle_, Uptane::RepositoryType::Director(), Uptane::Role::Root())));
  image_root_version_ = Uptane::Version(Uptane::extractVersionUntrusted(
      Uptane::getMetaFromBundle(meta_bundle_, Uptane::RepositoryType::Image(), Uptane::Role::Root())));
}

void Metadata::fetchRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo, const Uptane::Role& role,
                         Uptane::Version version) const {
  (void)maxsize;

  getRoleMetadata(result, repo, role, version);
}

void Metadata::fetchLatestRole(std::string* result, int64_t maxsize, Uptane::RepositoryType repo,
                               const Uptane::Role& role) const {
  (void)maxsize;
  getRoleMetadata(result, repo, role, Uptane::Version());
}

void Metadata::getRoleMetadata(std::string* result, const Uptane::RepositoryType& repo, const Uptane::Role& role,
                               Uptane::Version version) const {
  if (role == Uptane::Role::Root() && version != Uptane::Version()) {
    // If requesting a Root version beyond what we have available, fail as
    // expected. If requesting a version before what is available, just use what
    // is available, since root rotation isn't supported here.
    if (repo == Uptane::RepositoryType::Director() && director_root_version_ < version) {
      LOG_DEBUG << "Requested Director root version " << version << " but only version " << director_root_version_
                << " is available.";
      throw std::runtime_error("Metadata not found");
    } else if (repo == Uptane::RepositoryType::Image() && image_root_version_ < version) {
      LOG_DEBUG << "Requested Image repo root version " << version << " but only version " << image_root_version_
                << " is available.";
      throw std::runtime_error("Metadata not found");
    }
  }

  *result = Uptane::getMetaFromBundle(meta_bundle_, repo, role);
}
