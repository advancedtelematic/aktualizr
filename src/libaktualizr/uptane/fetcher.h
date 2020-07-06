#ifndef UPTANE_FETCHER_H_
#define UPTANE_FETCHER_H_

#include "http/httpinterface.h"
#include "libaktualizr/config.h"
#include "storage/invstorage.h"

namespace Uptane {

constexpr int64_t kMaxRootSize = 64 * 1024;
constexpr int64_t kMaxDirectorTargetsSize = 64 * 1024;
constexpr int64_t kMaxTimestampSize = 64 * 1024;
constexpr int64_t kMaxSnapshotSize = 64 * 1024;
constexpr int64_t kMaxImageTargetsSize = 8 * 1024 * 1024;

class IMetadataFetcher {
 public:
  IMetadataFetcher(const IMetadataFetcher&) = delete;
  IMetadataFetcher& operator=(const IMetadataFetcher&) = delete;
  virtual ~IMetadataFetcher() = default;

 public:
  virtual void fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, const Uptane::Role& role,
                         Version version) const = 0;
  virtual void fetchLatestRole(std::string* result, int64_t maxsize, RepositoryType repo,
                               const Uptane::Role& role) const = 0;

 protected:
  IMetadataFetcher() = default;
  IMetadataFetcher(IMetadataFetcher&&) = default;
};

class Fetcher : public IMetadataFetcher {
 public:
  Fetcher(const Config& config_in, std::shared_ptr<HttpInterface> http_in)
      : Fetcher(config_in.uptane.repo_server, config_in.uptane.director_server, std::move(http_in)) {}
  Fetcher(std::string repo_server_in, std::string director_server_in, std::shared_ptr<HttpInterface> http_in)
      : http(std::move(http_in)),
        repo_server(std::move(repo_server_in)),
        director_server(std::move(director_server_in)) {}
  void fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, const Uptane::Role& role,
                 Version version) const override;
  void fetchLatestRole(std::string* result, int64_t maxsize, RepositoryType repo,
                       const Uptane::Role& role) const override {
    fetchRole(result, maxsize, repo, role, Version());
  }

  std::string getRepoServer() const { return repo_server; }

 private:
  std::shared_ptr<HttpInterface> http;
  std::string repo_server;
  std::string director_server;
};

}  // namespace Uptane

#endif
