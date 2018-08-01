#include "fetcher.h"

#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif

namespace Uptane {

bool Fetcher::fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, Uptane::Role role, Version version) {
  // TODO: chain-loading root.json
  std::string base_url = (repo == RepositoryType::Director) ? config.uptane.director_server : config.uptane.repo_server;
  std::string url = base_url + "/" + version.RoleFileName(role);
  HttpResponse response = http->get(url, maxsize);
  if (!response.isOk()) {
    return false;
  }
  *result = response.body;
  return true;
}

static size_t DownloadHandler(char* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  auto* ds = static_cast<Uptane::DownloadMetaStruct*>(userp);
  uint64_t downloaded = size * nmemb;
  auto expected = static_cast<uint64_t>(ds->target.length());
  if ((ds->downloaded_length + downloaded) > expected) {
    return ds->downloaded_length + downloaded;  // curl will abort if return unexpected size;
  }

  // incomplete writes will stop the download (written_size != nmemb*size)
  size_t written_size = ds->fhandle->wfeed(reinterpret_cast<uint8_t*>(contents), downloaded);
  ds->hasher().update(reinterpret_cast<const unsigned char*>(contents), written_size);
  unsigned int calculated = 0;
  if (loggerGetSeverity() <= boost::log::trivial::severity_level::info) {
    if (ds->downloaded_length > 0) {
      std::cout << "\r";
    }
    ds->downloaded_length += downloaded;
    calculated = static_cast<unsigned int>((ds->downloaded_length * 100) / expected);
    std::cout << "Downloading: " << calculated << "%";
    if (ds->downloaded_length == expected) {
      std::cout << "\n";
    }
  }

  if (ds->events_channel != nullptr) {
    auto event = std::make_shared<event::DownloadProgressReport>(ds->target, "Downloading", calculated);
    *ds->events_channel << event;
  }
  return written_size;
}

bool Fetcher::fetchVerifyTarget(const Target& target) {
  try {
    if (!target.IsOstree()) {
      DownloadMetaStruct ds(target, events_channel);
      std::unique_ptr<StorageTargetWHandle> fhandle =
          storage->allocateTargetFile(false, target.filename(), static_cast<size_t>(target.length()));
      ds.fhandle = fhandle.get();
      ds.downloaded_length = 0;

      if (target.hashes().empty()) {
        throw Exception("image", "No hash defined for the target");
      }

      HttpResponse response =
          http->download(config.uptane.repo_server + "/targets/" + target.filename(), DownloadHandler, &ds);
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
    } else {
#ifdef BUILD_OSTREE
      KeyManager keys(storage, config.keymanagerConfig());
      keys.loadKeys();
      OstreeManager::pull(config.pacman.sysroot, config.pacman.ostree_server, keys, target, events_channel);
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
