#include "fetcher.h"

#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif

namespace Uptane {

std::string Fetcher::fetchRole(const std::string& base_url, Uptane::Role role, Version version) {
  // TODO: chain-loading root.json
  std::string url = base_url + "/" + version.RoleFileName(role);
  HttpResponse response = http.get(url);
  if (!response.isOk()) {
    throw MissingRepo(url);
  }
  return response.body;
}

bool Fetcher::fetchRoot(bool director, Version version) {
  std::string root;
  try {
    root = fetchRole((director) ? config.uptane.director_server : config.uptane.repo_server, Role::Root(), version);
  } catch (MissingRepo()) {
    LOG_WARNING << "Can't connect to the repo";
    return false;
  }
  storage->storeUncheckedRoot(director, root, version);
  return true;
}

bool Fetcher::fetchMeta() {
  RawMetaPack meta;

  try {
    meta.director_root = fetchRole(config.uptane.director_server, Role::Root());
    meta.image_root = fetchRole(config.uptane.repo_server, Role::Root());

    meta.image_timestamp = fetchRole(config.uptane.repo_server, Role::Timestamp(), Version());

    // TODO split fetchMeta into more fine-grained functions to optimize network traffic
    meta.image_snapshot = fetchRole(config.uptane.repo_server, Role::Snapshot(), Version());
    meta.image_targets = fetchRole(config.uptane.repo_server, Role::Targets(), Version());

    meta.director_targets = fetchRole(config.uptane.director_server, Role::Targets(), Version());
  } catch (MissingRepo()) {
    LOG_WARNING << "Can't connect to the repo";
    return false;
  }

  storage->storeUncheckedMetadata(meta);
  return true;
}

static size_t DownloadHandler(char* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  auto* ds = static_cast<Uptane::DownloadMetaStruct*>(userp);
  ds->downloaded_length += size * nmemb;
  if (ds->downloaded_length > ds->expected_length) {
    return (size * nmemb) + 1;  // curl will abort if return unexpected size;
  }

  // incomplete writes will stop the download (written_size != nmemb*size)
  size_t written_size = ds->fhandle->wfeed(reinterpret_cast<uint8_t*>(contents), nmemb * size);
  ds->hasher().update(reinterpret_cast<const unsigned char*>(contents), written_size);
  return written_size;
}

bool Fetcher::fetchTarget(const Target& target) {
  try {
    if (target.length() > 0) {
      DownloadMetaStruct ds;
      std::unique_ptr<StorageTargetWHandle> fhandle =
          storage->allocateTargetFile(false, target.filename(), target.length());
      ds.fhandle = fhandle.get();
      ds.downloaded_length = 0;
      ds.expected_length = target.length();

      if (target.hashes().empty()) {
        throw Exception("image", "No hash defined for the target");
      }

      ds.hash_type = target.hashes()[0].type();
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

      if (!target.MatchWith(Hash(ds.hash_type, ds.hasher().getHexDigest()))) {
        throw TargetHashMismatch(target.filename());
      }
    } else if (target.format().empty() || target.format() == "OSTREE") {
#ifdef BUILD_OSTREE
      KeyManager keys(storage, config.keymanagerConfig());
      keys.loadKeys();
      OstreeManager::pull(config.pacman.sysroot, config.pacman.ostree_server, keys, target.sha256Hash());
#else
      LOG_ERROR << "Could not pull OSTree target. Aktualizr was built without OSTree support!";
      return false;
#endif
    }
  } catch (const Exception& e) {
    LOG_WARNING << "Error while downloading a target: " << e.what();
    return false;
  }
  return true;
}

}  // namespace Uptane
