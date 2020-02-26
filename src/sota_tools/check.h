#ifndef SOTA_CLIENT_TOOLS_CHECK_H_
#define SOTA_CLIENT_TOOLS_CHECK_H_

#include <string>

#include "garage_common.h"
#include "ostree_ref.h"
#include "ostree_repo.h"
#include "server_credentials.h"

/**
 * Check if the ref is present on the server and in targets.json
 */
int CheckRefValid(TreehubServer& treehub, const std::string& ref, RunMode mode, int max_curl_requests,
                  const boost::filesystem::path& tree_dir = "");

#endif
