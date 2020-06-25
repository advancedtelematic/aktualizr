#include "bootstrap.h"

#include <stdio.h>
#include <fstream>
#include <sstream>

#include "crypto/crypto.h"
#include "logging/logging.h"
#include "utilities/utils.h"

Bootstrap::Bootstrap(const boost::filesystem::path& provision_path, const std::string& provision_password)
    : ca_(""), cert_(""), pkey_("") {
  if (provision_path.empty()) {
    LOG_ERROR << "Provision path is empty!";
    throw std::runtime_error("Unable to parse bootstrap (shared) credentials");
  }

  std::ifstream as(provision_path.c_str(), std::ios::in | std::ios::binary);
  if (as.fail()) {
    LOG_ERROR << "Unable to open provided provisioning archive " << provision_path << ": " << std::strerror(errno);
    throw std::runtime_error("Unable to parse bootstrap (shared) credentials");
  }

  std::string p12_str = Utils::readFileFromArchive(as, "autoprov_credentials.p12");
  if (p12_str.empty()) {
    throw std::runtime_error("Unable to parse bootstrap (shared) credentials");
  }

  readTlsP12(p12_str, provision_password, pkey_, cert_, ca_);
}

void Bootstrap::readTlsP12(const std::string& p12_str, const std::string& provision_password, std::string& pkey,
                           std::string& cert, std::string& ca) {
  StructGuard<BIO> reg_p12(BIO_new_mem_buf(p12_str.c_str(), static_cast<int>(p12_str.size())), BIO_vfree);
  if (reg_p12 == nullptr) {
    LOG_ERROR << "Unable to open P12 archive: " << std::strerror(errno);
    throw std::runtime_error("Unable to parse bootstrap credentials");
  }

  if (!Crypto::parseP12(reg_p12.get(), provision_password, &pkey, &cert, &ca)) {
    LOG_ERROR << "Unable to parse P12 archive";
    throw std::runtime_error("Unable to parse bootstrap credentials");
  }
}

std::string Bootstrap::readServerUrl(const boost::filesystem::path& provision_path) {
  std::string url;
  try {
    std::ifstream as(provision_path.c_str(), std::ios::in | std::ios::binary);
    if (as.fail()) {
      LOG_ERROR << "Unable to open provided provisioning archive " << provision_path << ": " << std::strerror(errno);
      throw std::runtime_error("Unable to parse bootstrap credentials");
    }
    url = Utils::readFileFromArchive(as, "autoprov.url", true);
  } catch (std::runtime_error& exc) {
    LOG_ERROR << "Unable to read server URL from archive: " << exc.what();
    url = "";
  }

  return url;
}

std::string Bootstrap::readServerCa(const boost::filesystem::path& provision_path) {
  std::string server_ca;
  try {
    std::ifstream as(provision_path.c_str(), std::ios::in | std::ios::binary);
    if (as.fail()) {
      LOG_ERROR << "Unable to open provided provisioning archive " << provision_path << ": " << std::strerror(errno);
      throw std::runtime_error("Unable to parse bootstrap credentials");
    }
    server_ca = Utils::readFileFromArchive(as, "server_ca.pem");
  } catch (std::runtime_error& exc) {
    LOG_ERROR << "Unable to read server CA certificate from archive: " << exc.what();
    return "";
  }

  return server_ca;
}
