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
    LOG_ERROR << "Loading initial Root metadata failed: " << e.what();
    return false;
  }
  return true;
}

bool RepositoryCommon::verifyRoot(const std::string& root_raw) {
  try {
    int prev_version = rootVersion();
    // 5.4.4.3.2.3. Version N+1 of the Root metadata file MUST have been signed
    // by the following: (1) a threshold of keys specified in the latest Root
    // metadata file (version N), and (2) a threshold of keys specified in the
    // new Root metadata file being validated (version N+1).
    root = Root(type, Utils::parseJSON(root_raw), root);  // double signature verification
    // 5.4.4.3.2.4. The version number of the latest Root metadata file (version
    // N) must be less than or equal to the version number of the new Root
    // metadata file (version N+1). NOTE: we do not accept an equal version
    // number. It must increment.
    if (root.version() != prev_version + 1) {
      LOG_ERROR << "Version in Root metadata doesn't match the expected value";
      return false;
    }
  } catch (const std::exception& e) {
    LOG_ERROR << "Signature verification for Root metadata failed: " << e.what();
    return false;
  }
  return true;
}

void RepositoryCommon::resetRoot() { root = Root(Root::Policy::kAcceptAll); }

bool RepositoryCommon::updateRoot(INvStorage& storage, const IMetadataFetcher& fetcher,
                                  const RepositoryType repo_type) {
  // 5.4.4.3.1. Load the previous Root metadata file.
  {
    std::string root_raw;
    if (storage.loadLatestRoot(&root_raw, repo_type)) {
      if (!initRoot(root_raw)) {
        return false;
      }
    } else {
      // This block is a hack only required as long as we have to explicitly
      // fetch this to make the Director recognize new devices as "online".
      if (repo_type == RepositoryType::Director()) {
        if (!fetcher.fetchLatestRole(&root_raw, kMaxRootSize, repo_type, Role::Root())) {
          return false;
        }
      }

      if (!fetcher.fetchRole(&root_raw, kMaxRootSize, repo_type, Role::Root(), Version(1))) {
        return false;
      }
      if (!initRoot(root_raw)) {
        return false;
      }
      storage.storeRoot(root_raw, repo_type, Version(1));
    }
  }

  // 5.4.4.3.2. Update to the latest Root metadata file.
  for (int version = rootVersion() + 1; version < kMaxRotations; ++version) {
    // 5.4.4.3.2.2. Try downloading a new version N+1 of the Root metadata file.
    std::string root_raw;
    if (!fetcher.fetchRole(&root_raw, kMaxRootSize, repo_type, Role::Root(), Version(version))) {
      break;
    }

    if (!verifyRoot(root_raw)) {
      return false;
    }

    // 5.4.4.3.2.5. Set the latest Root metadata file to the new Root metadata
    // file.
    storage.storeRoot(root_raw, repo_type, Version(version));
    storage.clearNonRootMeta(repo_type);
  }

  // 5.4.4.3.3. Check that the current (or latest securely attested) time is
  // lower than the expiration timestamp in the latest Root metadata file.
  // (Checks for a freeze attack.)
  return !rootExpired();
}

}  // namespace Uptane
