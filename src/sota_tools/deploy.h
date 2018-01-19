#ifndef SOTA_CLIENT_TOOLS_DEPLOY_H_
#define SOTA_CLIENT_TOOLS_DEPLOY_H_

#include <string>
#include "ostree_ref.h"
#include "server_credentials.h"

/**
 * Upload a OSTree repository to Treehub.
 * This will not push the root reference to /refs/heads/qemux86-64 or update
 * the Uptane targets using garage-sign.
 * \param src_repo Maybe either a OSTreeDirRepo (in which case the objects
 *                 are fetched from disk), or OSTreeHttpRepo (in which case
 *                 the objects will be pulled over https).
 */
bool UploadToTreehub(const OSTreeRepo::ptr src_repo, const ServerCredentials& push_credentials,
                     const OSTreeHash& ostree_commit, const std::string& cacerts, const bool dryrun,
                     const int max_curl_requests);

/**
 * Use the garage-sign tool and the images targets.json keys in credentials.zip
 * to add an entry to images/targets.json
 */
bool OfflineSignRepo(const ServerCredentials& push_credentials, const std::string& name, const OSTreeHash& hash,
                     const std::string& hardwareids);

/**
 * Update images/targets.json by pushing the OSTree commit hash to /refs/heads/qemux86-64
 */
bool PushRootRef(const ServerCredentials& push_credentials, const OSTreeRef& ref, const std::string& cacerts,
                 bool dry_run);

#endif
