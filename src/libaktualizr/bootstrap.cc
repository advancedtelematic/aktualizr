#include "bootstrap.h"

#include <boost/algorithm/string.hpp>

#include <stdio.h>
#include <fstream>
#include <sstream>

#include "logging/logging.h"
#include "utilities/crypto.h"
#include "utilities/utils.h"

Bootstrap::Bootstrap(const boost::filesystem::path& provision_path, const std::string& provision_password)
    : ca(""), cert(""), pkey("") {
  if (provision_path.empty()) {
    LOG_ERROR << "Provision path is empty!";
    throw std::runtime_error("Unable to parse bootstrap credentials");
  }

  std::ifstream as(provision_path.c_str(), std::ios::in | std::ios::binary);
  if (as.fail()) {
    LOG_ERROR << "Unable to open provided provision archive '" << provision_path << "': " << std::strerror(errno);
    throw std::runtime_error("Unable to parse bootstrap credentials");
  }

  std::string p12_str = Utils::readFileFromArchive(as, "autoprov_credentials.p12");
  if (p12_str.empty()) {
    throw std::runtime_error("Unable to parse bootstrap credentials");
  }

  StructGuard<BIO> reg_p12(BIO_new_mem_buf(p12_str.c_str(), p12_str.size()), BIO_vfree);
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
      LOG_ERROR << "Unable to open provided provision archive '" << provision_path << "': " << std::strerror(errno);
      throw std::runtime_error("Unable to parse bootstrap credentials");
    }
    url = Utils::readFileFromArchive(as, "autoprov.url");
    boost::trim(url);
  } catch (std::runtime_error& exc) {
    LOG_ERROR << "Unable to read server url from archive: " << exc.what();
    url = "";
  }

  return url;
}

std::string Bootstrap::readServerCa(const boost::filesystem::path& provision_path) {
  std::string server_ca;
  try {
    std::ifstream as(provision_path.c_str(), std::ios::in | std::ios::binary);
    if (as.fail()) {
      LOG_ERROR << "Unable to open provided provision archive '" << provision_path << "': " << std::strerror(errno);
      throw std::runtime_error("Unable to parse bootstrap credentials");
    }
    server_ca = Utils::readFileFromArchive(as, "server_ca.pem");
  } catch (std::runtime_error& exc) {
    LOG_ERROR << "Unable to read server ca from archive: " << exc.what();
    return "";
  }

  return server_ca;
}
