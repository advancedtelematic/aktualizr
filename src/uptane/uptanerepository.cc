#include "uptane/uptanerepository.h"

#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/make_shared.hpp>

#include "bootstrap.h"
#include "crypto.h"
#include "invstorage.h"
#include "logging.h"
#include "openssl_compat.h"
#include "utils.h"

#ifdef BUILD_OSTREE
#include "ostree.h"
#endif

namespace Uptane {

static size_t DownloadHandler(char* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  Uptane::DownloadMetaStruct* ds = static_cast<Uptane::DownloadMetaStruct*>(userp);
  ds->downloaded_length += size * nmemb;
  if (ds->downloaded_length > ds->expected_length) {
    return (size * nmemb) + 1;  // curl will abort if return unexpected size;
  }

  // incomplete writes will stop the download (written_size != nmemb*size)
  size_t written_size = ds->fhandle->wfeed(reinterpret_cast<uint8_t*>(contents), nmemb * size);
  ds->sha256_hasher.update((const unsigned char*)contents, written_size);
  ds->sha512_hasher.update((const unsigned char*)contents, written_size);
  return written_size;
}

Repository::Repository(const Config& config_in, boost::shared_ptr<INvStorage> storage_in, HttpInterface& http_client)
    : config(config_in),
      storage(storage_in),
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
  manifest["ecu_version_manifest"] = version_manifests;

  Json::Value tuf_signed = keys_.signTuf(manifest);
  HttpResponse response = http.put(config.uptane.director_server + "/manifest", tuf_signed);
  return response.isOk();
}

Json::Value Repository::signVersionManifest(const Json::Value& primary_version_manifests) {
  Json::Value ecu_version_signed = keys_.signTuf(primary_version_manifests);
  return ecu_version_signed;
}

// Check for consistency, signatures are already checked
bool Repository::verifyMeta(const Uptane::MetaPack& meta) {
  // verify that director and image targets are consistent
  for (std::vector<Uptane::Target>::const_iterator it = meta.director_targets.targets.begin();
       it != meta.director_targets.targets.end(); ++it) {
    std::vector<Uptane::Target>::const_iterator image_target_it;
    image_target_it = std::find(meta.image_targets.targets.begin(), meta.image_targets.targets.end(), *it);
    if (image_target_it == meta.image_targets.targets.end()) {
      LOG_WARNING << "Target " << it->filename() << " in director repo not found in image repo.";
      LOG_WARNING << it->toJson();
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
  MetaPack meta;
  TimeStamp now(TimeStamp::Now());

  meta.director_root =
      Uptane::Root(now, "director", fetchRole(config.uptane.director_server, Role::Root()), meta_.director_root);
  meta.image_root = Uptane::Root(now, "repo", fetchRole(config.uptane.repo_server, Role::Root()), meta_.image_root);

  meta.image_timestamp = Uptane::TimestampMeta(
      now, "repo", fetchRole(config.uptane.repo_server, Role::Timestamp(), Version()), meta_.image_root);
  if (meta.image_timestamp.version() > meta_.image_timestamp.version()) {
    meta.image_snapshot = Uptane::Snapshot(
        now, "repo", fetchRole(config.uptane.repo_server, Role::Snapshot(), Version()), meta_.image_root);
    meta.image_targets = Uptane::Targets(now, "repo", fetchRole(config.uptane.repo_server, Role::Targets(), Version()),
                                         meta_.image_root);
  } else {
    meta.image_snapshot = meta_.image_snapshot;
    meta.image_targets = meta_.image_targets;
  }

  meta.director_targets = Uptane::Targets(
      now, "director", fetchRole(config.uptane.director_server, Role::Targets(), Version()), meta_.director_root);
  if (meta.director_root.version() > meta_.director_root.version() ||
      meta.image_root.version() > meta.image_root.version() ||
      meta.director_targets.version() > meta_.director_targets.version() ||
      meta.image_timestamp.version() > meta_.image_timestamp.version()) {
    if (verifyMeta(meta_)) {
      storage->storeMetadata(meta_);
      meta_ = meta;
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
    for (std::vector<Uptane::Target>::iterator it = director_targets.begin(); it != director_targets.end(); ++it) {
      // TODO: support downloading encrypted targets from director
      downloadTarget(*it);
    }
  }
  return std::pair<uint32_t, std::vector<Uptane::Target> >(version, director_targets);
}

void Repository::downloadTarget(Target target) {
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
      } else {
        throw Exception("image", "Could not download file, error: " + response.error_message);
      }
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
    OstreeManager::pull(config, keys, target.sha256Hash());
#else
    LOG_ERROR << "Could not pull OSTree target. Aktualizr was built without OSTree support!";
#endif
  }
}
}  // namespace Uptane
