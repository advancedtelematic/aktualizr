#ifndef UPTANE_FETCHER_H_
#define UPTANE_FETCHER_H_

#include "config/config.h"
#include "http/httpinterface.h"
#include "storage/invstorage.h"
#include "utilities/events.h"

namespace Uptane {

constexpr int64_t kMaxRootSize = 64 * 1024;
constexpr int64_t kMaxDirectorTargetsSize = 64 * 1024;
constexpr int64_t kMaxTimestampSize = 64 * 1024;
constexpr int64_t kMaxSnapshotSize = 64 * 1024;
constexpr int64_t kMaxImagesTargetsSize = 1024 * 1024;
struct DownloadMetaStruct {
  DownloadMetaStruct(Target target_in, std::shared_ptr<event::Channel> events_channel_in)
      : hash_type{target_in.hashes()[0].type()},
        events_channel{std::move(events_channel_in)},
        target{std::move(target_in)} {}
  uint64_t downloaded_length{};
  StorageTargetWHandle* fhandle{};
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
  std::shared_ptr<event::Channel> events_channel;
  Target target;

 private:
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

class Fetcher {
 public:
  Fetcher(const Config& config_in, std::shared_ptr<INvStorage> storage_in, std::shared_ptr<HttpInterface> http_in,
          std::shared_ptr<event::Channel> events_channel_in = nullptr)
      : http(std::move(http_in)),
        storage(std::move(storage_in)),
        config(config_in),
        events_channel(std::move(events_channel_in)) {}
  bool fetchVerifyTarget(const Target& target);
  bool fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, Uptane::Role role, Version version);
  bool fetchLatestRole(std::string* result, int64_t maxsize, RepositoryType repo, Uptane::Role role) {
    return fetchRole(result, maxsize, repo, role, Version());
  }

 private:
  std::shared_ptr<HttpInterface> http;
  std::shared_ptr<INvStorage> storage;
  const Config& config;
  std::shared_ptr<event::Channel> events_channel;
};

}  // namespace Uptane

#endif
