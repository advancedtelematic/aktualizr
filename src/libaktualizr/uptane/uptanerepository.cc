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
  } catch (...) {
    LOG_ERROR << "Loading initial root failed";
    throw;
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
  } catch (...) {
    LOG_ERROR << "Signature verification for root metadata failed";
    return false;
  }
  return true;
}

void RepositoryCommon::resetRoot() { root = Root(Root::Policy::kAcceptAll); }

Json::Value Manifest::signManifest(const Json::Value& version_manifests) {
  Json::Value manifest;
  manifest["primary_ecu_serial"] = primary_ecu_serial.ToString();
  manifest["ecu_version_manifests"] = version_manifests;

  return keys_.signTuf(manifest);
}

Json::Value Manifest::signVersionManifest(const Json::Value& primary_version_manifests) {
  Json::Value ecu_version_signed = keys_.signTuf(primary_version_manifests);
  return ecu_version_signed;
}

}  // namespace Uptane
