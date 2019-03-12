#include "packagemanagerinterface.h"

#include "http/httpclient.h"
#include "logging/logging.h"

struct DownloadMetaStruct {
  DownloadMetaStruct(Uptane::Target target_in, FetcherProgressCb progress_cb_in, const api::FlowControlToken* token_in)
      : hash_type{target_in.hashes()[0].type()},
        target{std::move(target_in)},
        token{token_in},
        progress_cb{std::move(progress_cb_in)} {}
  uint64_t downloaded_length{0};
  unsigned int last_progress{0};
  std::unique_ptr<StorageTargetWHandle> fhandle;
  const Uptane::Hash::Type hash_type;
  MultiPartHasher& hasher() {
    switch (hash_type) {
      case Uptane::Hash::Type::kSha256:
        return sha256_hasher;
      case Uptane::Hash::Type::kSha512:
        return sha512_hasher;
      default:
        throw std::runtime_error("Unknown hash algorithm");
    }
  }
  Uptane::Target target;
  const api::FlowControlToken* token;
  FetcherProgressCb progress_cb;

 private:
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

static size_t DownloadHandler(char* contents, size_t size, size_t nmemb, void* userp) {
  assert(userp);
  auto ds = static_cast<DownloadMetaStruct*>(userp);
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
  auto ds = static_cast<DownloadMetaStruct*>(clientp);

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

static void restoreHasherState(MultiPartHasher& hasher, StorageTargetRHandle* data) {
  size_t data_len;
  size_t buf_len = 1024;
  uint8_t buf[buf_len];
  do {
    data_len = data->rread(buf, buf_len);
    hasher.update(buf, data_len);
  } while (data_len != 0);
}

bool PackageManagerBase::fetchTarget(const Uptane::Target& target, const std::string& repo_server,
                                     const KeyManager& keys, FetcherProgressCb progress_cb,
                                     const api::FlowControlToken* token) {
  (void)keys;
  bool result = false;
  try {
    if (target.hashes().empty()) {
      throw Uptane::Exception("image", "No hash defined for the target");
    }
    auto target_exists = storage_->checkTargetFile(target);
    if (target_exists && (*target_exists).first == target.length()) {
      LOG_INFO << "Image already downloaded skipping download";
      return true;
    }
    DownloadMetaStruct ds(target, progress_cb, token);
    if (target_exists) {
      ds.downloaded_length = target_exists->first;
      auto target_handle = storage_->openTargetFile(target);
      ::restoreHasherState(ds.hasher(), target_handle.get());
      target_handle->rclose();
      ds.fhandle = target_handle->toWriteHandle();
    } else {
      ds.fhandle = storage_->allocateTargetFile(false, target);
    }
    HttpResponse response;
    for (;;) {
      response = http_->download(repo_server + "/targets/" + Utils::urlEncode(target.filename()), DownloadHandler,
                                 ProgressHandler, &ds, static_cast<curl_off_t>(ds.downloaded_length));
      if (!response.wasAborted()) {
        break;
      }
      ds.fhandle.reset();
      if (!token->canContinue()) {
        break;
      }
      ds.fhandle = storage_->openTargetFile(target)->toWriteHandle();
    }
    LOG_TRACE << "Download status: " << response.getStatusStr() << std::endl;
    if (!response.isOk()) {
      if (response.curl_code == CURLE_WRITE_ERROR) {
        throw Uptane::OversizedTarget(target.filename());
      }
      throw Uptane::Exception("image", "Could not download file, error: " + response.error_message);
    }
    if (!target.MatchWith(Uptane::Hash(ds.hash_type, ds.hasher().getHexDigest()))) {
      ds.fhandle->wabort();
      throw Uptane::TargetHashMismatch(target.filename());
    }
    ds.fhandle->wcommit();
    result = true;
  } catch (const Uptane::Exception& e) {
    LOG_WARNING << "Error while downloading a target: " << e.what();
  }
  return result;
}
