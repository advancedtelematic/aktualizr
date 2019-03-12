#ifndef UPTANE_FETCHER_H_
#define UPTANE_FETCHER_H_

#include "config/config.h"
#include "http/httpinterface.h"
#include "storage/invstorage.h"

namespace Uptane {

constexpr int64_t kMaxRootSize = 64 * 1024;
constexpr int64_t kMaxDirectorTargetsSize = 64 * 1024;
constexpr int64_t kMaxTimestampSize = 64 * 1024;
constexpr int64_t kMaxSnapshotSize = 64 * 1024;
constexpr int64_t kMaxImagesTargetsSize = 1024 * 1024;

class Fetcher {
 public:
  Fetcher(const Config& config_in, std::shared_ptr<HttpInterface> http_in)
      : http(std::move(http_in)), config(config_in) {}
  bool fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, const Uptane::Role& role, Version version);
  bool fetchLatestRole(std::string* result, int64_t maxsize, RepositoryType repo, const Uptane::Role& role) {
    return fetchRole(result, maxsize, repo, role, Version());
  }

 private:
  std::shared_ptr<HttpInterface> http;
  const Config& config;
};

}  // namespace Uptane

#endif
