#include "uptane/uptanerepository.h"

#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <utility>

#include "bootstrap/bootstrap.h"
#include "crypto/crypto.h"
#include "crypto/openssl_compat.h"
#include "logging/logging.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

namespace Uptane {

Repository::Repository(const Config& config_in, std::shared_ptr<INvStorage> storage_in)
    : config(config_in),
      storage(std::move(storage_in)),
      keys_(storage, config.keymanagerConfig()),
      manifests(Json::arrayValue) {
  RawMetaPack meta_stored;

  meta_.director_root = Root(Root::kAcceptAll);
  meta_.image_root = Root(Root::kAcceptAll);
  if (storage->loadMetadata(&meta_stored)) {
    // stored metadata is trusted
    meta_.director_root = Uptane::Root("director", Utils::parseJSON(meta_stored.director_root));
    meta_.image_root = Uptane::Root("repo", Utils::parseJSON(meta_stored.image_root));

    meta_.image_timestamp = Uptane::TimestampMeta(Utils::parseJSON(meta_stored.image_timestamp));
    meta_.image_snapshot = Uptane::Snapshot(Utils::parseJSON(meta_stored.image_snapshot));
    meta_.image_targets = Uptane::Targets(Utils::parseJSON(meta_stored.image_targets));

    meta_.director_targets = Uptane::Targets(Utils::parseJSON(meta_stored.director_targets));
  }
}

Json::Value Repository::signManifest(const Json::Value& version_manifests) {
  Json::Value manifest;
  manifest["primary_ecu_serial"] = primary_ecu_serial;
  manifest["ecu_version_manifests"] = version_manifests;

  return keys_.signTuf(manifest);
}

Json::Value Repository::signVersionManifest(const Json::Value& primary_version_manifests) {
  Json::Value ecu_version_signed = keys_.signTuf(primary_version_manifests);
  return ecu_version_signed;
}

bool comapreForConsistency(const Uptane::Target& director_target, const Uptane::Target& image_target) {
  if (director_target.length() > image_target.length()) {
    return false;
  }
  if (director_target.filename() != image_target.filename()) {
    return false;
  }
  if (director_target.hashes().size() != image_target.hashes().size()) {
    return false;
  }
  // TODO: hardware_identifier and release_counter?
  for (auto const& director_hash : director_target.hashes()) {
    // FIXME std:find doesn't work here, can't understand why.
    // auto found = std::find(image_target.hashes().begin(),
    //                        image_target.hashes().end(), director_hash);
    // if (found == image_target.hashes().end()) return false;

    bool found = false;
    for (auto const& imh : image_target.hashes()) {
      if (director_hash == imh) {
        found = true;
        break;
      };
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

// Check for consistency, signatures are already checked
bool Repository::verifyMetaTargets(const Uptane::Targets& director_targets, const Uptane::Targets& image_targets) {
  // verify that director and image targets are consistent
  for (auto director_target : director_targets.targets) {
    auto image_target_it = std::find_if(image_targets.targets.begin(), image_targets.targets.end(),
                                        std::bind(comapreForConsistency, director_target, std::placeholders::_1));
    if (image_target_it == image_targets.targets.end()) {
      LOG_WARNING << "Target " << director_target.filename() << " in director repo not found in image repo.";
      LOG_DEBUG << director_target.toJson();
      return false;
    }
  }
  return true;
}

bool Repository::feedCheckMeta() {
  TimeStamp now(TimeStamp::Now());
  MetaPack new_meta;
  RawMetaPack meta_raw;

  // should be already put to the storage by fetcher
  if (!storage->loadUncheckedMetadata(&meta_raw)) {
    LOG_WARNING << "No metadata to check";
    return false;
  }

  try {
    // first check with previous root
    new_meta.director_root =
        Uptane::Root(now, "director", Utils::parseJSON(meta_raw.director_root), meta_.director_root);
    new_meta.image_root = Uptane::Root(now, "repo", Utils::parseJSON(meta_raw.image_root), meta_.image_root);

    // second check against the new root
    new_meta.director_root =
        Uptane::Root(now, "director", Utils::parseJSON(meta_raw.director_root), new_meta.director_root);
    new_meta.image_root = Uptane::Root(now, "repo", Utils::parseJSON(meta_raw.image_root), new_meta.image_root);

    new_meta.image_timestamp =
        Uptane::TimestampMeta(now, "repo", Utils::parseJSON(meta_raw.image_timestamp), new_meta.image_root);
    new_meta.image_snapshot =
        Uptane::Snapshot(now, "repo", Utils::parseJSON(meta_raw.image_snapshot), new_meta.image_root);
    new_meta.image_targets =
        Uptane::Targets(now, "repo", Utils::parseJSON(meta_raw.image_targets), new_meta.image_root);

    new_meta.director_targets =
        Uptane::Targets(now, "director", Utils::parseJSON(meta_raw.director_targets), new_meta.director_root);

    if (new_meta.director_root.version() < meta_.director_root.version() ||
        new_meta.image_root.version() < meta_.image_root.version() ||
        new_meta.director_targets.version() < meta_.director_targets.version() ||
        new_meta.image_targets.version() < meta_.image_targets.version() ||
        new_meta.image_snapshot.version() < meta_.image_snapshot.version() ||
        new_meta.image_timestamp.version() < meta_.image_timestamp.version()) {
      LOG_WARNING << "Metadata downgrade attempted";
      return false;
    }

    if (!verifyMetaTargets(new_meta.director_targets, new_meta.image_targets)) {
      LOG_WARNING << "Metadata image/directory repo consistency check failed";
      return false;
    }

    if (!storage->loadMetadata(nullptr) || new_meta.director_root.version() > meta_.director_root.version() ||
        new_meta.image_root.version() > meta_.image_root.version() ||
        new_meta.director_targets.version() > meta_.director_targets.version() ||
        new_meta.image_targets.version() > meta_.image_targets.version() ||
        new_meta.image_snapshot.version() > meta_.image_snapshot.version() ||
        new_meta.image_timestamp.version() > meta_.image_timestamp.version()) {
      storage->storeMetadata(meta_raw);
      meta_ = new_meta;
    }
  } catch (const Exception& e) {
    // save exception to use in vector tests
    last_exception = e;
    LOG_WARNING << "Uptane exception in " << e.getName() << ": " << e.what();
    return false;
  }
  return true;
}

bool Repository::feedCheckRoot(bool director, Version version) {
  std::string root;
  if (storage->loadUncheckedRoot(director, &root, version)) {
    return false;
  }
  // TODO: verify
  if (director)
    meta_.director_root = Uptane::Root("director", Utils::parseJSON(root));
  else
    meta_.image_root = Uptane::Root("repo", Utils::parseJSON(root));

  storage->storeRoot(director, root, version);

  return true;
}

std::pair<int, std::vector<Uptane::Target> > Repository::getTargets() {
  std::vector<Uptane::Target> director_targets = meta_.director_targets.targets;
  int version = meta_.director_targets.version();

  return std::pair<uint32_t, std::vector<Uptane::Target> >(version, director_targets);
}

}  // namespace Uptane
