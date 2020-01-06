#include "fetcher.h"

namespace Uptane {

bool Fetcher::fetchRole(std::string* result, int64_t maxsize, RepositoryType repo, const Uptane::Role& role,
                        Version version) const {
  std::string url = (repo == RepositoryType::Director()) ? director_server : repo_server;
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

}  // namespace Uptane
