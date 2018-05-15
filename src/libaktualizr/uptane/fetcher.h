#ifndef UPTANE_FETCHER_H_
#define UPTANE_FETCHER_H_

#include "config/config.h"
#include "http/httpinterface.h"
#include "storage/invstorage.h"

namespace Uptane {

struct DownloadMetaStruct {
  int64_t expected_length{};
  int64_t downloaded_length{};
  StorageTargetWHandle* fhandle{};
  Hash::Type hash_type{Hash::kUnknownAlgorithm};
  MultiPartHasher& hasher() {
    switch (hash_type) {
      case Hash::kSha256:
        return sha256_hasher;
      case Hash::kSha512:
        return sha512_hasher;
      default:
        throw std::runtime_error("Unknown hash algorithm");
    }
  }
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

class Fetcher {
 public:
  Fetcher(const Config& config, const std::shared_ptr<INvStorage>& storage, HttpInterface& http)
      : http(http), storage(storage), config(config) {}
  bool fetchMeta();
  bool fetchRoot(bool director, Version version);
  bool fetchTarget(const Target& target);

 private:
  std::string fetchRole(const std::string& base_url, Uptane::Role role, Version version = Version());
  HttpInterface& http;
  std::shared_ptr<INvStorage> storage;
  const Config& config;
};

}  // namespace Uptane

#endif
