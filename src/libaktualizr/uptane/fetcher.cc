#include "fetcher.h"

#ifdef BUILD_OSTREE
#include "package_manager/ostreemanager.h"  // TODO: Hide behind PackageManagerInterface
#endif

namespace Uptane {

bool Fetcher::fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, const Uptane::Role& role,
                        Version version) {
  // TODO: chain-loading root.json
  std::string url = (repo == RepositoryType::Director()) ? config.uptane.director_server : config.uptane.repo_server;
  if (role.IsDelegation()) {
    url += "/delegations";
  }
  url += "/" + version.RoleFileName(role);
  HttpResponse response = http->get(url, maxsize);
  if (!response.isOk()) {
    return false;
  }
  *result = response.body;
  return true;
}

static size_t DownloadHandler(char* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  auto ds = static_cast<Uptane::DownloadMetaStruct*>(userp);
  uint64_t downloaded = size * nmemb;
  uint64_t expected = ds->target.length();
  if ((ds->downloaded_length + downloaded) > expected) {
    return downloaded + 1;  // curl will abort if return unexpected size;
  }

  // incomplete writes will stop the download (written_size != nmemb*size)
  size_t written_size = ds->fhandle->wfeed(reinterpret_cast<uint8_t*>(contents), downloaded);
  ds->hasher().update(reinterpret_cast<const unsigned char*>(contents), written_size);

  ds->downloaded_length += downloaded;
  return written_size;
}

static int ProgressHandler(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
  (void)dltotal;
  (void)dlnow;
  (void)ultotal;
  (void)ulnow;
  auto ds = static_cast<Uptane::DownloadMetaStruct*>(clientp);

  uint64_t expected = ds->target.length();
  auto progress = static_cast<unsigned int>((ds->downloaded_length * 100) / expected);
  if (ds->progress_cb && progress > ds->last_progress) {
    ds->last_progress = progress;
    ds->progress_cb(ds->target, "Downloading", progress);
  }
  if (ds->token != nullptr && !ds->token->canContinue(false)) {
    return 1;
  }
  return 0;
}

void Fetcher::restoreHasherState(MultiPartHasher& hasher, StorageTargetRHandle* data) {
  size_t data_len;
  size_t buf_len = 1024;
  uint8_t buf[buf_len];
  do {
    data_len = data->rread(buf, buf_len);
    hasher.update(buf, data_len);
  } while (data_len != 0);
}

bool Fetcher::fetchVerifyTarget(const Target& target, const api::FlowControlToken* token) {
  bool result = false;
  try {
    if (!target.IsOstree()) {
      if (target.hashes().empty()) {
        throw Exception("image", "No hash defined for the target");
      }
      auto target_exists = storage->checkTargetFile(target);
      if (target_exists && (*target_exists).first == target.length()) {
        LOG_INFO << "Image already downloaded skipping download";
        return true;
      }
      DownloadMetaStruct ds(target, progress_cb, token);
      if (target_exists) {
        ds.downloaded_length = target_exists->first;
        auto target_handle = storage->openTargetFile(target);
        restoreHasherState(ds.hasher(), target_handle.get());
        target_handle->rclose();
        ds.fhandle = target_handle->toWriteHandle();
      } else {
        ds.fhandle = storage->allocateTargetFile(false, target);
      }
      HttpResponse response;
      for (;;) {
        response = http->download(config.uptane.repo_server + "/targets/" + Utils::urlEncode(target.filename()),
                                  DownloadHandler, ProgressHandler, &ds, static_cast<curl_off_t>(ds.downloaded_length));
        if (!response.wasAborted()) {
          break;
        }
        ds.fhandle.reset();
        if (!token->canContinue()) {
          break;
        }
        ds.fhandle = storage->openTargetFile(target)->toWriteHandle();
      }
      LOG_TRACE << "Download status: " << response.getStatusStr() << std::endl;
      if (!response.isOk()) {
        if (response.curl_code == CURLE_WRITE_ERROR) {
          throw OversizedTarget(target.filename());
        }
        throw Exception("image", "Could not download file, error: " + response.error_message);
      }
      if (!target.MatchWith(Hash(ds.hash_type, ds.hasher().getHexDigest()))) {
        ds.fhandle->wabort();
        throw TargetHashMismatch(target.filename());
      }
      ds.fhandle->wcommit();
      result = true;
    } else {
#ifdef BUILD_OSTREE
      KeyManager keys(storage, config.keymanagerConfig());
      keys.loadKeys();
      data::InstallationResult install_res =
          OstreeManager::pull(config.pacman.sysroot, config.pacman.ostree_server, keys, target, token, progress_cb);
      result = install_res.success;
#else
      LOG_ERROR << "Could not pull OSTree target. Aktualizr was built without OSTree support!";
#endif
    }
  } catch (const Exception& e) {
    LOG_WARNING << "Error while downloading a target: " << e.what();
  }
  return result;
}

}  // namespace Uptane
