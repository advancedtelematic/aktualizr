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
    int prev_version = root.version();
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

Json::Value Manifest::signManifest(const Json::Value& manifest_unsigned) const {
  Json::Value manifest = keys_.signTuf(manifest_unsigned);
  return manifest;
}

}  // namespace Uptane
