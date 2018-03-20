#include "uptane/tufrepository.h"

#include <fcntl.h>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "crypto.h"
#include "keymanager.h"
#include "logging.h"
#ifdef BUILD_OSTREE
#include "ostree.h"
#endif
#include "utils.h"

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

TufRepository::TufRepository(const std::string& name, const std::string& base_url, const Config& config,
                             boost::shared_ptr<INvStorage>& storage, HttpInterface& http_client)
    : name_(name),
      config_(config),
      storage_(storage),
      http_(http_client),
      base_url_(base_url),
      root_(Root::kRejectAll) {
  Uptane::MetaPack meta;
  if (storage_->loadMetadata(&meta)) {
    if (name_ == "repo") {
      root_ = meta.image_root;
      targets_ = meta.image_targets;
      timestamp_ = meta.image_timestamp;
      snapshot_ = meta.image_snapshot;
    } else if (name == "director") {
      root_ = meta.director_root;
      targets_ = meta.director_targets;
    }
  } else {
    root_ = Root(Root::kAcceptAll);
  }
}

Json::Value TufRepository::getJSON(const std::string& role) {
  HttpResponse response = http_.get(base_url_ + "/" + role);
  if (!response.isOk()) {
    throw MissingRepo(name_);
  }
  return response.getJson();
}

void TufRepository::updateRoot(Version version) {
  Json::Value new_root_json = fetchAndCheckRole(Role::Root(), version);
  // Validation passed
  root_ = Root(name_, new_root_json);
}

/**
 * Note this doesn't check the validity of version numbers, only that the signed part is actually signed.
 * @param role
 * @param version
 * @param root_used
 * @return The contents of the "signed" section
 */
Json::Value TufRepository::fetchAndCheckRole(Uptane::Role role, Version version, Uptane::Root* root_used) {
  // TODO: chain-loading root.json
  TimeStamp now(TimeStamp::Now());
  Json::Value content = getJSON(version.RoleFileName(role));
  return verifyRole(role, now, content, root_used);
}

Json::Value TufRepository::verifyRole(Uptane::Role role, const TimeStamp& now, const Json::Value& content,
                                      Uptane::Root* root_used) {
  if (!root_used) root_used = &root_;
  Json::Value result = root_used->UnpackSignedObject(now, name_, role, content);
  if (role == Role::Root()) {
    // Also check that the new root is suitably self-signed
    Root new_root = Root(name_, result);
    new_root.UnpackSignedObject(now, name_, Role::Root(), content);
  }
  return result;
}

std::string TufRepository::downloadTarget(Target target) {
  DownloadMetaStruct ds;
  std::unique_ptr<StorageTargetWHandle> fhandle =
      storage_->allocateTargetFile(false, target.filename(), target.length());
  ds.fhandle = fhandle.get();
  ds.downloaded_length = 0;
  ds.expected_length = target.length();

  HttpResponse response = http_.download(base_url_ + "/targets/" + target.filename(), DownloadHandler, &ds);
  if (!response.isOk()) {
    fhandle->wabort();
    if (response.curl_code == CURLE_WRITE_ERROR) {
      throw OversizedTarget(target.filename());
    } else {
      throw Exception(name_, "Could not download file, error: " + response.error_message);
    }
  }
  fhandle->wcommit();
  std::string h256 = ds.sha256_hasher.getHexDigest();
  std::string h512 = ds.sha512_hasher.getHexDigest();

  if (!target.MatchWith(Hash(Hash::kSha256, h256)) && !target.MatchWith(Hash(Hash::kSha512, h512))) {
    throw TargetHashMismatch(target.filename());
  }
  return response.body;
}

void TufRepository::saveTarget(const Target& target) {
  if (target.length() > 0) {
    downloadTarget(target);
  } else if (target.format().empty() || target.format() == "OSTREE") {
#ifdef BUILD_OSTREE
    KeyManager keys(storage_, config_);
    keys.loadKeys();
    OstreeManager::pull(config_, keys, target.sha256Hash());
#else
    LOG_ERROR << "Could not pull OSTree target. Aktualizr was built without OSTree support!";
#endif
  }
}

void TufRepository::setMeta(Uptane::Root* root, Uptane::Targets* targets, Uptane::TimestampMeta* timestamp,
                            Uptane::Snapshot* snapshot) {
  if (root) root_ = *root;
  if (targets) targets_ = *targets;
  if (timestamp) timestamp_ = *timestamp;
  if (snapshot) snapshot_ = *snapshot;
}
}  // namespace Uptane
