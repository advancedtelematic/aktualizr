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
  auto* ds = static_cast<Uptane::DownloadMetaStruct*>(userp);
  uint64_t downloaded = size * nmemb;
  uint64_t expected = ds->target.length();
  if ((ds->downloaded_length + downloaded) > expected) {
    return downloaded + 1;  // curl will abort if return unexpected size;
  }

  // incomplete writes will stop the download (written_size != nmemb*size)
  size_t written_size = ds->fhandle->wfeed(reinterpret_cast<uint8_t*>(contents), downloaded);
  ds->hasher().update(reinterpret_cast<const unsigned char*>(contents), written_size);

  ds->downloaded_length += downloaded;
  auto progress = static_cast<unsigned int>((ds->downloaded_length * 100) / expected);
  if (ds->progress_cb && progress > ds->last_progress) {
    ds->last_progress = progress;
    ds->progress_cb(ds->target, "Downloading", progress);
  }
  if (ds->fetcher->isPaused()) {
    ds->fetcher->setRetry(true);
    ds->fhandle->wcommit();
    ds->fhandle.reset();
    return written_size + 1;  // Abort downloading because pause is requested.
  }
  return written_size;
}

Fetcher::PauseRet Fetcher::setPause(bool pause) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (pause_ == pause) {
    if (pause) {
      LOG_DEBUG << "Fetcher: already paused";
      return PauseRet::kAlreadyPaused;
    } else {
      LOG_DEBUG << "Fetcher: nothing to resume";
      return PauseRet::kNotPaused;
    }
  }

  pause_ = pause;
  cv_.notify_all();

  if (pause) {
    return PauseRet::kPaused;
  } else {
    return PauseRet::kResumed;
  }
}

void Fetcher::checkPause() {
  std::unique_lock<std::mutex> lk(mutex_);
  cv_.wait(lk, [this] { return !pause_; });
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

bool Fetcher::fetchVerifyTarget(const Target& target) {
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
      DownloadMetaStruct ds(target, progress_cb);
      ds.fetcher = this;
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
      do {
        checkPause();
        if (retry_) {
          retry_ = false;
          // fhandle was invalidated on pause
          ds.fhandle = storage->openTargetFile(target)->toWriteHandle();
        }
        response = http->download(config.uptane.repo_server + "/targets/" + Utils::urlEncode(target.filename()),
                                  DownloadHandler, &ds, static_cast<curl_off_t>(ds.downloaded_length));
        LOG_TRACE << "Download status: " << response.getStatusStr() << std::endl;
      } while (retry_);
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
      std::function<void()> check_pause = std::bind(&Fetcher::checkPause, this);
      data::InstallationResult install_res = OstreeManager::pull(config.pacman.sysroot, config.pacman.ostree_server,
                                                                 keys, target, check_pause, progress_cb);
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
