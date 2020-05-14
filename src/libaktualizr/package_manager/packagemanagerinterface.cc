#include "packagemanagerinterface.h"

#include <sys/statvfs.h>

#include "http/httpclient.h"
#include "logging/logging.h"

struct DownloadMetaStruct {
  DownloadMetaStruct(Uptane::Target target_in, FetcherProgressCb progress_cb_in, const api::FlowControlToken* token_in)
      : hash_type{target_in.hashes()[0].type()},
        target{std::move(target_in)},
        token{token_in},
        progress_cb{std::move(progress_cb_in)} {}
  uintmax_t downloaded_length{0};
  unsigned int last_progress{0};
  std::ofstream fhandle;
  const Hash::Type hash_type;
  MultiPartHasher& hasher() {
    switch (hash_type) {
      case Hash::Type::kSha256:
        return sha256_hasher;
      case Hash::Type::kSha512:
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
  auto* ds = static_cast<DownloadMetaStruct*>(userp);
  size_t downloaded = size * nmemb;
  uint64_t expected = ds->target.length();
  if ((ds->downloaded_length + downloaded) > expected) {
    return downloaded + 1;  // curl will abort if return unexpected size;
  }

  ds->fhandle.write(contents, static_cast<std::streamsize>(downloaded));
  ds->hasher().update(reinterpret_cast<const unsigned char*>(contents), downloaded);
  ds->downloaded_length += downloaded;
  return downloaded;
}

static int ProgressHandler(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
  (void)dltotal;
  (void)dlnow;
  (void)ultotal;
  (void)ulnow;
  auto* ds = static_cast<DownloadMetaStruct*>(clientp);

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

static void restoreHasherState(MultiPartHasher& hasher, std::ifstream data) {
  static constexpr size_t buf_len = 1024;
  std::array<uint8_t, buf_len> buf{};
  do {
    data.read(reinterpret_cast<char*>(buf.data()), buf.size());
    hasher.update(buf.data(), static_cast<uint64_t>(data.gcount()));
  } while (data.gcount() != 0);
}

bool PackageManagerInterface::fetchTarget(const Uptane::Target& target, Uptane::Fetcher& fetcher,
                                          const KeyManager& keys, const FetcherProgressCb& progress_cb,
                                          const api::FlowControlToken* token) {
  (void)keys;
  bool result = false;
  try {
    if (target.hashes().empty()) {
      throw Uptane::Exception("image", "No hash defined for the target");
    }
    TargetStatus exists = PackageManagerInterface::verifyTarget(target);
    if (exists == TargetStatus::kGood) {
      LOG_INFO << "Image already downloaded; skipping download";
      return true;
    }
    std::unique_ptr<DownloadMetaStruct> ds = std_::make_unique<DownloadMetaStruct>(target, progress_cb, token);
    if (target.length() == 0) {
      LOG_INFO << "Skipping download of target with length 0";
      ds->fhandle = createTargetFile(target);
      return true;
    }
    if (exists == TargetStatus::kIncomplete) {
      LOG_INFO << "Continuing incomplete download of file " << target.filename();
      auto target_check = checkTargetFile(target);
      ds->downloaded_length = target_check->first;
      ::restoreHasherState(ds->hasher(), openTargetFile(target));
      ds->fhandle = appendTargetFile(target);
    } else {
      // If the target was found, but is oversized or the hash doesn't match,
      // just start over.
      LOG_DEBUG << "Initiating download of file " << target.filename();
      ds->fhandle = createTargetFile(target);
    }

    const uint64_t required_bytes = target.length() - ds->downloaded_length;
    if (!checkAvailableDiskSpace(required_bytes)) {
      throw std::runtime_error("Insufficient disk space available to download target");
    }

    std::string target_url = target.uri();
    if (target_url.empty()) {
      target_url = fetcher.getRepoServer() + "/targets/" + Utils::urlEncode(target.filename());
    }

    HttpResponse response;
    for (;;) {
      response = http_->download(target_url, DownloadHandler, ProgressHandler, ds.get(),
                                 static_cast<curl_off_t>(ds->downloaded_length));

      if (response.curl_code == CURLE_RANGE_ERROR) {
        LOG_WARNING << "The image server doesn't support byte range requests,"
                       " try to download the image from the beginning: "
                    << target_url;
        ds = std_::make_unique<DownloadMetaStruct>(target, progress_cb, token);
        ds->fhandle = createTargetFile(target);
        continue;
      }

      if (!response.wasInterrupted()) {
        break;
      }
      ds->fhandle.close();
      // sleep if paused or abort the download
      if (!token->canContinue()) {
        throw Uptane::Exception("image", "Download of a target was aborted");
      }
      ds->fhandle = appendTargetFile(target);
    }
    LOG_TRACE << "Download status: " << response.getStatusStr() << std::endl;
    if (!response.isOk()) {
      if (response.curl_code == CURLE_WRITE_ERROR) {
        throw Uptane::OversizedTarget(target.filename());
      }
      throw Uptane::Exception("image", "Could not download file, error: " + response.error_message);
    }
    if (!target.MatchHash(Hash(ds->hash_type, ds->hasher().getHexDigest()))) {
      ds->fhandle.close();
      removeTargetFile(target);
      throw Uptane::TargetHashMismatch(target.filename());
    }
    ds->fhandle.close();
    result = true;
  } catch (const std::exception& e) {
    LOG_WARNING << "Error while downloading a target: " << e.what();
  }
  return result;
}

TargetStatus PackageManagerInterface::verifyTarget(const Uptane::Target& target) const {
  auto target_exists = checkTargetFile(target);
  if (!target_exists) {
    LOG_DEBUG << "File " << target.filename() << " with expected hash not found in the database.";
    return TargetStatus::kNotFound;
  } else if (target_exists->first < target.length()) {
    LOG_DEBUG << "File " << target.filename() << " was found in the database, but is incomplete.";
    return TargetStatus::kIncomplete;
  } else if (target_exists->first > target.length()) {
    LOG_DEBUG << "File " << target.filename() << " was found in the database, but is oversized.";
    return TargetStatus::kOversized;
  }

  // Even if the file exists and the length matches, recheck the hash.
  DownloadMetaStruct ds(target, nullptr, nullptr);
  ds.downloaded_length = target_exists->first;
  ::restoreHasherState(ds.hasher(), openTargetFile(target));
  if (!target.MatchHash(Hash(ds.hash_type, ds.hasher().getHexDigest()))) {
    LOG_ERROR << "Target exists with expected length, but hash does not match metadata! " << target;
    return TargetStatus::kHashMismatch;
  }

  return TargetStatus::kGood;
}

bool PackageManagerInterface::checkAvailableDiskSpace(const uint64_t required_bytes) const {
  struct statvfs stvfsbuf {};
  const int stat_res = statvfs(config.images_path.c_str(), &stvfsbuf);
  if (stat_res < 0) {
    LOG_WARNING << "Unable to read filesystem statistics: error code " << stat_res;
    return true;
  }
  const uint64_t available_bytes = (static_cast<uint64_t>(stvfsbuf.f_bsize) * stvfsbuf.f_bavail);
  const uint64_t reserved_bytes = 1 << 20;

  if (required_bytes + reserved_bytes < available_bytes) {
    return true;
  } else {
    LOG_ERROR << "Insufficient disk space available to download target! Required: " << required_bytes
              << ", available: " << available_bytes << ", reserved: " << reserved_bytes;
    return false;
  }
}

boost::optional<std::pair<uintmax_t, std::string>> PackageManagerInterface::checkTargetFile(
    const Uptane::Target& target) const {
  std::string filename = storage_->getTargetFilename(target.filename());
  if (!filename.empty()) {
    auto path = config.images_path / filename;
    if (boost::filesystem::exists(path)) {
      return {{boost::filesystem::file_size(path), path.string()}};
    }
  }
  return boost::none;
}

std::ifstream PackageManagerInterface::openTargetFile(const Uptane::Target& target) const {
  auto file = checkTargetFile(target);
  if (!file) {
    throw std::runtime_error("File doesn't exist for target " + target.filename());
  }
  std::ifstream stream(file->second, std::ios::binary);
  if (!stream.good()) {
    throw std::runtime_error("Can't open file " + file->second);
  }
  return stream;
}

std::ofstream PackageManagerInterface::createTargetFile(const Uptane::Target& target) {
  std::string filename = target.hashes()[0].HashString();
  std::string filepath = (config.images_path / filename).string();
  boost::filesystem::create_directories(config.images_path);
  std::ofstream stream(filepath, std::ios::binary | std::ios::ate);
  if (!stream.good()) {
    throw std::runtime_error("Can't write to file " + filepath);
  }
  storage_->storeTargetFilename(target.filename(), filename);
  return stream;
}

std::ofstream PackageManagerInterface::appendTargetFile(const Uptane::Target& target) {
  auto file = checkTargetFile(target);
  if (!file) {
    throw std::runtime_error("File doesn't exist for target " + target.filename());
  }
  std::ofstream stream(file->second, std::ios::binary | std::ios::app);
  if (!stream.good()) {
    throw std::runtime_error("Can't open file " + file->second);
  }
  return stream;
}

void PackageManagerInterface::removeTargetFile(const Uptane::Target& target) {
  auto file = checkTargetFile(target);
  if (!file) {
    throw std::runtime_error("File doesn't exist for target " + target.filename());
  }
  boost::filesystem::remove(file->second);
  storage_->deleteTargetInfo(target.filename());
}

std::vector<Uptane::Target> PackageManagerInterface::getTargetFiles() {
  std::vector<Uptane::Target> v;
  auto names = storage_->getAllTargetNames();
  v.reserve(names.size());
  for (const auto& name : names) {
    v.emplace_back(name, Uptane::EcuMap{}, std::vector<Hash>{}, 0);
  }
  return v;
}
