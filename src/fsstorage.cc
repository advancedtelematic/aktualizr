#include "fsstorage.h"

#include <boost/scoped_array.hpp>
#include <iostream>

#include "logger.h"
#include "utils.h"

FSStorage::FSStorage(const Config& config) : config_(config) {
  if (!boost::filesystem::exists(config_.tls.certificates_directory))
    boost::filesystem::create_directories(config_.tls.certificates_directory);
  if (!boost::filesystem::exists(config_.uptane.metadata_path))
    boost::filesystem::create_directories(config_.uptane.metadata_path);
  if (!boost::filesystem::exists(config_.uptane.metadata_path / "repo"))
    boost::filesystem::create_directories(config_.uptane.metadata_path / "repo");
  if (!boost::filesystem::exists(config_.uptane.metadata_path / "director"))
    boost::filesystem::create_directories(config_.uptane.metadata_path / "director");
}
FSStorage::~FSStorage() {
  // TODO: clear director_files, image_files
}

void FSStorage::storeEcu(bool is_primary, const std::string& hardware_id __attribute__((unused)),
                         const std::string& ecu_id, const std::string& public_key, const std::string& private_key) {
  boost::filesystem::path public_key_path;
  boost::filesystem::path private_key_path;

  if (is_primary) {
    public_key_path = config_.uptane.public_key_path;
    private_key_path = config_.uptane.private_key_path;
  } else {
    public_key_path = config_.tls.certificates_directory / (ecu_id + ".pub");
    private_key_path = config_.tls.certificates_directory / (ecu_id + ".priv");
  }

  if (boost::filesystem::exists(public_key_path)) boost::filesystem::remove(public_key_path);

  if (boost::filesystem::exists(private_key_path)) boost::filesystem::remove(private_key_path);

  Utils::writeFile(public_key_path.native(), public_key);
  Utils::writeFile(private_key_path.native(), private_key);

  sync();
}

bool FSStorage::loadEcuKeys(bool is_primary, const std::string& hardware_id __attribute__((unused)),
                            const std::string& ecu_id, std::string* public_key, std::string* private_key) {
  boost::filesystem::path public_key_path;
  boost::filesystem::path private_key_path;

  if (is_primary) {
    public_key_path = config_.uptane.public_key_path;
    private_key_path = config_.uptane.private_key_path;
  } else {
    public_key_path = config_.tls.certificates_directory / (ecu_id + ".pub");
    private_key_path = config_.tls.certificates_directory / (ecu_id + ".priv");
  }

  if (!boost::filesystem::exists(public_key_path) || !boost::filesystem::exists(private_key_path)) return false;

  *public_key = Utils::readFile(public_key_path.native());
  *private_key = Utils::readFile(private_key_path.native());
  return true;
}

void FSStorage::storeTlsCredsCommon(const boost::filesystem::path& ca_path, const boost::filesystem::path& cert_path,
                                    const boost::filesystem::path& pkey_path, const std::string& ca,
                                    const std::string& cert, const std::string& pkey) {
  if (boost::filesystem::exists(ca_path)) boost::filesystem::remove(ca_path);

  if (boost::filesystem::exists(cert_path)) boost::filesystem::remove(cert_path);

  if (boost::filesystem::exists(pkey_path)) boost::filesystem::remove(pkey_path);

  Utils::writeFile(ca_path.native(), ca);
  Utils::writeFile(cert_path.native(), cert);
  Utils::writeFile(pkey_path.native(), pkey);

  sync();
}

void FSStorage::storeBootstrapTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  boost::filesystem::path ca_path = config_.tls.certificates_directory / "bootstrap_ca.pem";
  boost::filesystem::path cert_path = config_.tls.certificates_directory / "bootstrap_cert.pem";
  boost::filesystem::path pkey_path = config_.tls.certificates_directory / "bootstrap_pkey.pem";
  storeTlsCredsCommon(ca_path, cert_path, pkey_path, ca, cert, pkey);
}

void FSStorage::storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  boost::filesystem::path ca_path = config_.tls.certificates_directory / config_.tls.ca_file;
  boost::filesystem::path cert_path = config_.tls.certificates_directory / config_.tls.client_certificate;
  boost::filesystem::path pkey_path = config_.tls.certificates_directory / config_.tls.pkey_file;
  storeTlsCredsCommon(ca_path, cert_path, pkey_path, ca, cert, pkey);
}

bool FSStorage::loadTlsCredsCommon(const boost::filesystem::path& ca_path, const boost::filesystem::path& cert_path,
                                   const boost::filesystem::path& pkey_path, std::string* ca, std::string* cert,
                                   std::string* pkey) {
  if (!boost::filesystem::exists(ca_path) || !boost::filesystem::exists(cert_path) ||
      !boost::filesystem::exists(pkey_path))
    return false;

  *ca = Utils::readFile(ca_path.native());
  *cert = Utils::readFile(cert_path.native());
  *pkey = Utils::readFile(pkey_path.native());

  return false;
}

bool FSStorage::loadBootstrapTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  boost::filesystem::path ca_path = config_.tls.certificates_directory / "bootstrap_ca.pem";
  boost::filesystem::path cert_path = config_.tls.certificates_directory / "bootstrap_cert.pem";
  boost::filesystem::path pkey_path = config_.tls.certificates_directory / "bootstrap_pkey.pem";
  return loadTlsCredsCommon(ca_path, cert_path, pkey_path, ca, cert, pkey);
}

bool FSStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  boost::filesystem::path ca_path = config_.tls.certificates_directory / config_.tls.ca_file;
  boost::filesystem::path cert_path = config_.tls.certificates_directory / config_.tls.client_certificate;
  boost::filesystem::path pkey_path = config_.tls.certificates_directory / config_.tls.pkey_file;
  return loadTlsCredsCommon(ca_path, cert_path, pkey_path, ca, cert, pkey);
}

void FSStorage::storeMetadata(const Uptane::MetaPack& metadata) {
  boost::filesystem::path image_path = config_.uptane.metadata_path / "repo";
  boost::filesystem::path director_path = config_.uptane.metadata_path / "director";

  Utils::writeFile((director_path / "root.json").native(), metadata.director_root.toJson());
  Utils::writeFile((director_path / "targets.json").native(), metadata.director_targets.toJson());
  Utils::writeFile((image_path / "root.json").native(), metadata.image_root.toJson());
  Utils::writeFile((image_path / "targets.json").native(), metadata.image_targets.toJson());
  Utils::writeFile((image_path / "timestamp.json").native(), metadata.image_timestamp.toJson());
  Utils::writeFile((image_path / "snapshot.json").native(), metadata.image_snapshot.toJson());
  sync();
}

bool FSStorage::loadMetadata(Uptane::MetaPack* metadata) {
  boost::filesystem::path image_path = config_.uptane.metadata_path / "repo";
  boost::filesystem::path director_path = config_.uptane.metadata_path / "director";

  if (!boost::filesystem::exists(director_path / "root.json") ||
      !boost::filesystem::exists(director_path / "targets.json") ||
      !boost::filesystem::exists(image_path / "root.json") || !boost::filesystem::exists(image_path / "targets.json") ||
      !boost::filesystem::exists(image_path / "timestamp.json") ||
      !boost::filesystem::exists(image_path / "snapshot.json"))
    return false;

  Json::Value json = Utils::parseJSONFile(director_path / "root.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->director_root = Uptane::Root("director", json);

  json = Utils::parseJSONFile(director_path / "targets.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->director_targets = Uptane::Targets(json);

  json = Utils::parseJSONFile(image_path / "root.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_root = Uptane::Root("image", json);

  json = Utils::parseJSONFile(image_path / "targets.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_targets = Uptane::Targets(json);

  json = Utils::parseJSONFile(image_path / "timestamp.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_timestamp = Uptane::TimestampMeta(json);

  json = Utils::parseJSONFile(image_path / "snapshot.json");
  // compatibility with old clients, which store the whole metadata, not just the signed part
  if (json.isMember("signed") && json.isMember("signatures")) json = json["signed"];
  metadata->image_snapshot = Uptane::Snapshot(json);

  return true;
}
