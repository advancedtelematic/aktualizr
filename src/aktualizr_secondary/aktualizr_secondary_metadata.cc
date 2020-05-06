#include "aktualizr_secondary_metadata.h"

Metadata::Metadata(const Uptane::RawMetaPack& meta_pack)
    : director_metadata_{{Uptane::Role::ROOT, meta_pack.director_root},
                         {Uptane::Role::TARGETS, meta_pack.director_targets}},
      image_metadata_{
          {Uptane::Role::ROOT, meta_pack.image_root},
          {Uptane::Role::TIMESTAMP, meta_pack.image_timestamp},
          {Uptane::Role::SNAPSHOT, meta_pack.image_snapshot},
          {Uptane::Role::TARGETS, meta_pack.image_targets},
      } {
  director_root_version_ = Uptane::Version(Uptane::extractVersionUntrusted(meta_pack.director_root));
  image_root_version_ = Uptane::Version(Uptane::extractVersionUntrusted(meta_pack.image_root));
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
  const std::unordered_map<std::string, std::string>* metadata_map = nullptr;

  if (repo == Uptane::RepositoryType::Director()) {
    metadata_map = &director_metadata_;
  } else if (repo == Uptane::RepositoryType::Image()) {
    metadata_map = &image_metadata_;
  }

  if (metadata_map == nullptr) {
    LOG_ERROR << "There are no any metadata for the given type of repository: " << repo.toString();
    throw std::runtime_error("TODO");
  }

  auto found_meta_it = metadata_map->find(role.ToString());
  if (found_meta_it == metadata_map->end()) {
    LOG_ERROR << "There are no any metadata for the given type of role: " << repo.toString() << ": " << role.ToString();
    throw std::runtime_error("TODO");
  }

  if (role == Uptane::Role::Root() && version != Uptane::Version()) {
    // If requesting a Root version beyond what we have available, fail as
    // expected. If requesting a version before what is available, just use what
    // is available, since root rotation isn't supported here.
    if (repo == Uptane::RepositoryType::Director() && director_root_version_ < version) {
      LOG_DEBUG << "Requested Director root version " << version << " but only version " << director_root_version_
                << " is available.";
      throw std::runtime_error("TODO");
    } else if (repo == Uptane::RepositoryType::Image() && image_root_version_ < version) {
      LOG_DEBUG << "Requested Image repo root version " << version << " but only version " << image_root_version_
                << " is available.";
      throw std::runtime_error("TODO");
    }
  }

  *result = found_meta_it->second;
}
