#include "uptane/uptanerepository.h"

#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <utility>

#include "bootstrap.h"
#include "logging/logging.h"
#include "openssl_compat.h"
#include "storage/invstorage.h"
#include "utilities/crypto.h"
#include "utilities/utils.h"

#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif

namespace Uptane {

static size_t DownloadHandler(char* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  auto* ds = static_cast<Uptane::DownloadMetaStruct*>(userp);
  ds->downloaded_length += size * nmemb;
  if (ds->downloaded_length > ds->expected_length) {
    return (size * nmemb) + 1;  // curl will abort if return unexpected size;
  }

  // incomplete writes will stop the download (written_size != nmemb*size)
  size_t written_size = ds->fhandle->wfeed(reinterpret_cast<uint8_t*>(contents), nmemb * size);
  ds->sha256_hasher.update(reinterpret_cast<const unsigned char*>(contents), written_size);
  ds->sha512_hasher.update(reinterpret_cast<const unsigned char*>(contents), written_size);
  return written_size;
}

Repository::Repository(const Config& config_in, std::shared_ptr<INvStorage> storage_in, HttpInterface& http_client)
    : config(config_in),
      storage(std::move(storage_in)),
      http(http_client),
      keys_(storage, config.keymanagerConfig()),
      manifests(Json::arrayValue) {
  if (!storage->loadMetadata(&meta_)) {
    meta_.director_root = Root(Root::kAcceptAll);
    meta_.image_root = Root(Root::kAcceptAll);
  }
}

void Repository::updateRoot(Version version) {
  Uptane::Root director_root("director", fetchRole(config.uptane.director_server, Role::Root(), version));
  meta_.director_root = director_root;
  Uptane::Root image_root("image", fetchRole(config.uptane.repo_server, Role::Root(), version));
  meta_.image_root = image_root;
}

bool Repository::putManifest(const Json::Value& version_manifests) {
  Json::Value manifest;
  manifest["primary_ecu_serial"] = primary_ecu_serial;
  manifest["ecu_version_manifests"] = version_manifests;

  Json::Value tuf_signed = keys_.signTuf(manifest);
  HttpResponse response = http.put(config.uptane.director_server + "/manifest", tuf_signed);
  return response.isOk();
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

Json::Value Repository::getJSON(const std::string& url) {
  HttpResponse response = http.get(url);
  if (!response.isOk()) {
    throw MissingRepo(url);
  }
  return response.getJson();
}

/**
 * Note this doesn't check the validity of version numbers, only that the signed part is actually signed.
 * @param repo
 * @param role
 * @param version
 * @param root_used
 * @return The contents of the "signed" section
 */
Json::Value Repository::fetchRole(const std::string& base_url, Uptane::Role role, Version version) {
  // TODO: chain-loading root.json
  return getJSON(base_url + "/" + version.RoleFileName(role));
}

bool Repository::getMeta() {
  MetaPack new_meta;
  TimeStamp now(TimeStamp::Now());

  new_meta.director_root =
      Uptane::Root(now, "director", fetchRole(config.uptane.director_server, Role::Root()), meta_.director_root);
  new_meta.image_root = Uptane::Root(now, "repo", fetchRole(config.uptane.repo_server, Role::Root()), meta_.image_root);

  new_meta.image_timestamp = Uptane::TimestampMeta(
      now, "repo", fetchRole(config.uptane.repo_server, Role::Timestamp(), Version()), meta_.image_root);
  if (new_meta.image_timestamp.version() > meta_.image_timestamp.version()) {
    new_meta.image_snapshot = Uptane::Snapshot(
        now, "repo", fetchRole(config.uptane.repo_server, Role::Snapshot(), Version()), meta_.image_root);
    new_meta.image_targets = Uptane::Targets(
        now, "repo", fetchRole(config.uptane.repo_server, Role::Targets(), Version()), meta_.image_root);
  } else {
    new_meta.image_snapshot = meta_.image_snapshot;
    new_meta.image_targets = meta_.image_targets;
  }

  new_meta.director_targets = Uptane::Targets(
      now, "director", fetchRole(config.uptane.director_server, Role::Targets(), Version()), meta_.director_root);
  if (new_meta.director_root.version() > meta_.director_root.version() ||
      new_meta.image_root.version() > meta_.image_root.version() ||
      new_meta.director_targets.version() > meta_.director_targets.version() ||
      new_meta.image_timestamp.version() > meta_.image_timestamp.version()) {
    if (verifyMetaTargets(new_meta.director_targets, new_meta.image_targets)) {
      storage->storeMetadata(new_meta);
      meta_ = new_meta;
    } else {
      LOG_WARNING << "Metadata image/directory repo consistency check failed.";
      return false;
    }
  }
  return true;
}

std::pair<int, std::vector<Uptane::Target> > Repository::getTargets() {
  std::vector<Uptane::Target> director_targets = meta_.director_targets.targets;
  int version = meta_.director_targets.version();

  if (!director_targets.empty()) {
    for (auto it = director_targets.begin(); it != director_targets.end(); ++it) {
      // TODO: support downloading encrypted targets from director
      downloadTarget(*it);
    }
  }
  return std::pair<uint32_t, std::vector<Uptane::Target> >(version, director_targets);
}

void Repository::downloadTarget(const Target& target) {
  if (target.length() > 0) {
    DownloadMetaStruct ds;
    std::unique_ptr<StorageTargetWHandle> fhandle =
        storage->allocateTargetFile(false, target.filename(), target.length());
    ds.fhandle = fhandle.get();
    ds.downloaded_length = 0;
    ds.expected_length = target.length();

    HttpResponse response =
        http.download(config.uptane.repo_server + "/targets/" + target.filename(), DownloadHandler, &ds);
    if (!response.isOk()) {
      fhandle->wabort();
      if (response.curl_code == CURLE_WRITE_ERROR) {
        throw OversizedTarget(target.filename());
      }
      throw Exception("image", "Could not download file, error: " + response.error_message);
    }
    fhandle->wcommit();
    std::string h256 = ds.sha256_hasher.getHexDigest();
    std::string h512 = ds.sha512_hasher.getHexDigest();

    if (!target.MatchWith(Hash(Hash::kSha256, h256)) && !target.MatchWith(Hash(Hash::kSha512, h512))) {
      throw TargetHashMismatch(target.filename());
    }
  } else if (target.format().empty() || target.format() == "OSTREE") {
#ifdef BUILD_OSTREE
    KeyManager keys(storage, config.keymanagerConfig());
    keys.loadKeys();
    OstreeManager::pull(config.pacman.sysroot, config.pacman.ostree_server, keys, target.sha256Hash());
#else
    LOG_ERROR << "Could not pull OSTree target. Aktualizr was built without OSTree support!";
#endif
  }
}
}  // namespace Uptane
