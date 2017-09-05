#include "fsstorage.h"

#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/scoped_array.hpp>

#include "logger.h"
#include "utils.h"

FSStorage::FSStorage(const Config& config) : config_(config) {
  boost::filesystem::create_directories(config_.tls.certificates_directory);
  boost::filesystem::create_directories(config_.uptane.metadata_path / "repo");
  boost::filesystem::create_directories(config_.uptane.metadata_path / "director");
}
FSStorage::~FSStorage() {
  // TODO: clear director_files, image_files
}

void FSStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  boost::filesystem::path public_key_path = config_.tls.certificates_directory / config_.uptane.public_key_path;
  boost::filesystem::path private_key_path = config_.tls.certificates_directory / config_.uptane.private_key_path;

  boost::filesystem::remove(public_key_path);
  boost::filesystem::remove(private_key_path);

  Utils::writeFile(public_key_path.string(), public_key);
  Utils::writeFile(private_key_path.string(), private_key);

  sync();
}

bool FSStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) {
  boost::filesystem::path public_key_path = config_.tls.certificates_directory / config_.uptane.public_key_path;
  boost::filesystem::path private_key_path = config_.tls.certificates_directory / config_.uptane.private_key_path;

  if (!boost::filesystem::exists(public_key_path) || !boost::filesystem::exists(private_key_path)) {
    return false;
  }

  if (public_key) *public_key = Utils::readFile(public_key_path.string());
  if (private_key) *private_key = Utils::readFile(private_key_path.string());
  return true;
}

void FSStorage::clearPrimaryKeys() {
  boost::filesystem::remove(config_.tls.certificates_directory / config_.uptane.public_key_path);
  boost::filesystem::remove(config_.tls.certificates_directory / config_.uptane.private_key_path);
}

void FSStorage::storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  boost::filesystem::path ca_path = config_.tls.certificates_directory / config_.tls.ca_file;
  boost::filesystem::path cert_path = config_.tls.certificates_directory / config_.tls.client_certificate;
  boost::filesystem::path pkey_path = config_.tls.certificates_directory / config_.tls.pkey_file;
  boost::filesystem::remove(ca_path);
  boost::filesystem::remove(cert_path);
  boost::filesystem::remove(pkey_path);

  Utils::writeFile(ca_path.string(), ca);
  Utils::writeFile(cert_path.string(), cert);
  Utils::writeFile(pkey_path.string(), pkey);

  sync();
}

bool FSStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  boost::filesystem::path ca_path = config_.tls.certificates_directory / config_.tls.ca_file;
  boost::filesystem::path cert_path = config_.tls.certificates_directory / config_.tls.client_certificate;
  boost::filesystem::path pkey_path = config_.tls.certificates_directory / config_.tls.pkey_file;
  if (!boost::filesystem::exists(ca_path) || !boost::filesystem::exists(cert_path) ||
      !boost::filesystem::exists(pkey_path))
    return false;
  if (ca) {
    *ca = Utils::readFile(ca_path.string());
  }
  if (cert) {
    *cert = Utils::readFile(cert_path.string());
  }
  if (pkey) {
    *pkey = Utils::readFile(pkey_path.string());
  }
  return true;
}

void FSStorage::clearTlsCreds() {
  boost::filesystem::remove(config_.tls.certificates_directory / config_.tls.ca_file);
  boost::filesystem::remove(config_.tls.certificates_directory / config_.tls.client_certificate);
  boost::filesystem::remove(config_.tls.certificates_directory / config_.tls.pkey_file);
}

bool FSStorage::loadTlsCa(std::string* ca) {
  boost::filesystem::path ca_path = config_.tls.certificates_directory / config_.tls.ca_file;
  if (!boost::filesystem::exists(ca_path)) return false;

  if (ca) {
    *ca = Utils::readFile(ca_path.string());
  }
  return true;
}

#ifdef BUILD_OSTREE
void FSStorage::storeMetadata(const Uptane::MetaPack& metadata) {
  boost::filesystem::path image_path = config_.uptane.metadata_path / "repo";
  boost::filesystem::path director_path = config_.uptane.metadata_path / "director";

  Utils::writeFile((director_path / "root.json").string(), metadata.director_root.toJson());
  Utils::writeFile((director_path / "targets.json").string(), metadata.director_targets.toJson());
  Utils::writeFile((image_path / "root.json").string(), metadata.image_root.toJson());
  Utils::writeFile((image_path / "targets.json").string(), metadata.image_targets.toJson());
  Utils::writeFile((image_path / "timestamp.json").string(), metadata.image_timestamp.toJson());
  Utils::writeFile((image_path / "snapshot.json").string(), metadata.image_snapshot.toJson());
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
#endif  // BUILD_OSTREE

void FSStorage::storeDeviceId(const std::string& device_id) {
  Utils::writeFile((config_.tls.certificates_directory / "device_id").string(), device_id);
}

bool FSStorage::loadDeviceId(std::string* device_id) {
  if (!boost::filesystem::exists((config_.tls.certificates_directory / "device_id").string())) return false;

  if (device_id) *device_id = Utils::readFile((config_.tls.certificates_directory / "device_id").string());
  return true;
}

void FSStorage::clearDeviceId() { boost::filesystem::remove(config_.tls.certificates_directory / "device_id"); }

void FSStorage::storeEcuRegistered() {
  Utils::writeFile((config_.tls.certificates_directory / "is_registered").string(), std::string("1"));
}

bool FSStorage::loadEcuRegistered() {
  return boost::filesystem::exists((config_.tls.certificates_directory / "is_registered").string());
}

void FSStorage::clearEcuRegistered() {
  boost::filesystem::remove(config_.tls.certificates_directory / "is_registered");
}

void FSStorage::storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) {
  if (serials.size() >= 1) {
    Utils::writeFile((config_.tls.certificates_directory / "primary_ecu_serial").string(), serials[0].first);
    Utils::writeFile((config_.tls.certificates_directory / "primary_ecu_hardware_id").string(), serials[0].second);

    boost::filesystem::remove_all((config_.tls.certificates_directory / "secondaries_list"));
    std::vector<std::pair<std::string, std::string> >::const_iterator it;
    std::ofstream file((config_.tls.certificates_directory / "secondaries_list").string().c_str());
    for (it = serials.begin() + 1; it != serials.end(); it++)
      // Assuming that there are no tabs and linebreaks in serials and hardware ids
      file << it->first << "\t" << it->second << "\n";
    file.close();
  }
}

bool FSStorage::loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) {
  std::string buf;
  std::string serial;
  std::string hw_id;
  if (!boost::filesystem::exists((config_.tls.certificates_directory / "primary_ecu_serial"))) return false;
  serial = Utils::readFile((config_.tls.certificates_directory / "primary_ecu_serial").string());
  // use default hardware ID for backwards compatibility
  if (!boost::filesystem::exists((config_.tls.certificates_directory / "primary_ecu_hardware_id")))
    hw_id = Utils::getHostname();
  else
    hw_id = Utils::readFile((config_.tls.certificates_directory / "primary_ecu_hardware_id").string());

  if (serials) serials->push_back(std::pair<std::string, std::string>(serial, hw_id));

  // return true for backwards compatibility
  if (!boost::filesystem::exists((config_.tls.certificates_directory / "secondaries_list"))) {
    return true;
  }
  std::ifstream file((config_.tls.certificates_directory / "secondaries_list").string().c_str());
  while (std::getline(file, buf)) {
    size_t tab = buf.find('\t');
    serial = buf.substr(0, tab);
    try {
      hw_id = buf.substr(tab + 1);
    } catch (const std::out_of_range& e) {
      if (serials) serials->clear();
      file.close();
      return false;
    }
    if (serials) serials->push_back(std::pair<std::string, std::string>(serial, hw_id));
  }
  file.close();
  return true;
}

void FSStorage::clearEcuSerials() {
  boost::filesystem::remove(config_.tls.certificates_directory / "primary_ecu_serial");
  boost::filesystem::remove(config_.tls.certificates_directory / "primary_hardware_id");
  boost::filesystem::remove(config_.tls.certificates_directory / "secondaries_list");
}
