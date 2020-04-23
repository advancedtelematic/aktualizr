#include "fetcher.h"

#include "uptane/exceptions.h"

namespace Uptane {

void Fetcher::fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, const Uptane::Role& role,
                        Version version) const {
  std::string url = (repo == RepositoryType::Director()) ? director_server : repo_server;
  if (role.IsDelegation()) {
    url += "/delegations";
  }
  url += "/" + version.RoleFileName(role);
  HttpResponse response = http->get(url, maxsize);
  if (!response.isOk()) {
    throw Uptane::Exception(repo.toString(), "Metadata fetch failure");
  }
  *result = response.body;
}

}  // namespace Uptane
