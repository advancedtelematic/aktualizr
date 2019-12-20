#include "uptane/uptanerepository.h"

#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <utility>

#include "bootstrap/bootstrap.h"
#include "crypto/crypto.h"
#include "crypto/openssl_compat.h"
#include "logging/logging.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

namespace Uptane {

bool RepositoryCommon::initRoot(const std::string& root_raw) {
  try {
    root = Root(type, Utils::parseJSON(root_raw));        // initialization and format check
    root = Root(type, Utils::parseJSON(root_raw), root);  // signature verification against itself
  } catch (const std::exception& e) {
    LOG_ERROR << "Loading initial root failed: " << e.what();
    return false;
  }
  return true;
}

bool RepositoryCommon::verifyRoot(const std::string& root_raw) {
  try {
    int prev_version = rootVersion();
    root = Root(type, Utils::parseJSON(root_raw), root);  // double signature verification
    if (root.version() != prev_version + 1) {
      LOG_ERROR << "Version in root metadata doesn't match the expected value";
      return false;
    }
  } catch (const std::exception& e) {
    LOG_ERROR << "Signature verification for root metadata failed: " << e.what();
    return false;
  }
  return true;
}

void RepositoryCommon::resetRoot() { root = Root(Root::Policy::kAcceptAll); }

bool RepositoryCommon::updateRoot(INvStorage& storage, const IMetadataFetcher& fetcher,
                                  const RepositoryType repo_type) {
  // Load current (or initial) Root metadata.
  {
    std::string root_raw;
    if (storage.loadLatestRoot(&root_raw, repo_type)) {
      if (!initRoot(root_raw)) {
        return false;
      }
    } else {
      if (!fetcher.fetchRole(&root_raw, kMaxRootSize, repo_type, Role::Root(), Version(1))) {
        return false;
      }
      if (!initRoot(root_raw)) {
        return false;
      }
      storage.storeRoot(root_raw, repo_type, Version(1));
    }
  }

  // Update to latest Root metadata.
  {
    // According to the current design root.json without a number is guaranteed to be the latest version.
    // Therefore we fetch the latest (root.json), and
    // if it matches what we already have stored, we're good.
    // If not, then we have to go fetch the missing ones by name/number until we catch up.

    std::string root_raw;
    if (!fetcher.fetchLatestRole(&root_raw, kMaxRootSize, repo_type, Role::Root())) {
      return false;
    }
    int remote_version = extractVersionUntrusted(root_raw);
    if (remote_version == -1) {
      LOG_ERROR << "Failed to extract a version from " << repo_type.toString() << "'s root.json: " << root_raw;
      return false;
    }

    int local_version = rootVersion();

    // if remote_version <= local_version then the root metadata are never verified
    // which leads to two issues
    // 1. At initial stage (just after provisioning) the root metadata from 1.root.json are not verified
    // 2. If local_version becomes higher than 1, e.g. 2 than a rollback attack is possible since the business logic
    // here won't return any error as suggested in #4 of
    // https://uptane.github.io/uptane-standard/uptane-standard.html#check_root
    // TODO: https://saeljira.it.here.com/browse/OTA-4119
    for (int version = local_version + 1; version <= remote_version; ++version) {
      if (!fetcher.fetchRole(&root_raw, kMaxRootSize, repo_type, Role::Root(), Version(version))) {
        return false;
      }

      if (!verifyRoot(root_raw)) {
        return false;
      }
      storage.storeRoot(root_raw, repo_type, Version(version));
      storage.clearNonRootMeta(repo_type);
    }

    // Check that the current (or latest securely attested) time is lower than the expiration timestamp in the latest
    // Root metadata file. (Checks for a freeze attack.)
    if (rootExpired()) {
      return false;
    }
  }

  return true;
}

Json::Value Manifest::signManifest(const Json::Value& manifest_unsigned) const {
  Json::Value manifest = keys_.signTuf(manifest_unsigned);
  return manifest;
}

}  // namespace Uptane
