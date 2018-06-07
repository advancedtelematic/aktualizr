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
  Hash::Type hash_type{Hash::Type::kUnknownAlgorithm};
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
  MultiPartSHA256Hasher sha256_hasher;
  MultiPartSHA512Hasher sha512_hasher;
};

class Fetcher {
 public:
  Fetcher(const Config& config_in, std::shared_ptr<INvStorage> storage_in, HttpInterface& http_in)
      : http(http_in), storage(std::move(storage_in)), config(config_in) {}
  // bool fetchMeta();
  // bool fetchRoot(bool director, Version version);
  bool fetchVerifyTarget(const Target& target);
  bool fetchRole(std::string* result, RepositoryType repo, Uptane::Role role, Version version = Version());

 private:
  HttpInterface& http;
  std::shared_ptr<INvStorage> storage;
  const Config& config;
};

}  // namespace Uptane

#endif
