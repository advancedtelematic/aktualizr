#include "fsstorage.h"

#include <iostream>

#include <boost/scoped_array.hpp>

#include "logging.h"
#include "utils.h"

FSStorage::FSStorage(const StorageConfig& config, const P11Config& p11_config)
    : INvStorage(p11_config), config_(config) {
  boost::filesystem::create_directories(Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo");
  boost::filesystem::create_directories(Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director");
}

FSStorage::~FSStorage() {
  // TODO: clear director_files, image_files
}

void FSStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  storePrimaryPublic(public_key);
  storePrimaryPrivate(private_key);
}

void FSStorage::storePrimaryPublic(const std::string& public_key) {
  boost::filesystem::path public_key_path = Utils::absolutePath(config_.path, config_.uptane_public_key_path);
  boost::filesystem::remove(public_key_path);
  Utils::writeFile(public_key_path, public_key);

  sync();
}

void FSStorage::storePrimaryPrivate(const std::string& private_key) {
  boost::filesystem::path private_key_path = Utils::absolutePath(config_.path, config_.uptane_private_key_path);
  boost::filesystem::remove(private_key_path);
  Utils::writeFile(private_key_path, private_key);

  sync();
}

bool FSStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) {
  return loadPrimaryPublic(public_key) && loadPrimaryPrivate(private_key);
}

bool FSStorage::loadPrimaryPublic(std::string* public_key) {
  boost::filesystem::path public_key_path = Utils::absolutePath(config_.path, config_.uptane_public_key_path);
  if (!boost::filesystem::exists(public_key_path)) {
    return false;
  }

  if (public_key) *public_key = Utils::readFile(public_key_path.string());
  return true;
}

bool FSStorage::loadPrimaryPrivate(std::string* private_key) {
  boost::filesystem::path private_key_path = Utils::absolutePath(config_.path, config_.uptane_private_key_path);
  if (!boost::filesystem::exists(private_key_path)) {
    return false;
  }

  if (private_key) *private_key = Utils::readFile(private_key_path.string());
  return true;
}

void FSStorage::clearPrimaryKeys() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.uptane_public_key_path));
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.uptane_private_key_path));
}

void FSStorage::storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  storeTlsCa(ca);
  storeTlsCert(cert);
  storeTlsPkey(pkey);
}

void FSStorage::storeTlsCa(const std::string& ca) {
  boost::filesystem::path ca_path(Utils::absolutePath(config_.path, config_.tls_cacert_path));
  boost::filesystem::remove(ca_path);
  Utils::writeFile(ca_path, ca);
  sync();
}

void FSStorage::storeTlsCert(const std::string& cert) {
  boost::filesystem::path cert_path(Utils::absolutePath(config_.path, config_.tls_clientcert_path));
  boost::filesystem::remove(cert_path);
  Utils::writeFile(cert_path, cert);
  sync();
}

void FSStorage::storeTlsPkey(const std::string& pkey) {
  boost::filesystem::path pkey_path(Utils::absolutePath(config_.path, config_.tls_pkey_path));
  boost::filesystem::remove(pkey_path);
  Utils::writeFile(pkey_path, pkey);
  sync();
}

bool FSStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  boost::filesystem::path ca_path(Utils::absolutePath(config_.path, config_.tls_cacert_path));
  boost::filesystem::path cert_path(Utils::absolutePath(config_.path, config_.tls_clientcert_path));
  boost::filesystem::path pkey_path(Utils::absolutePath(config_.path, config_.tls_pkey_path));
  if (!boost::filesystem::exists(ca_path) || boost::filesystem::is_directory(ca_path) ||
      !boost::filesystem::exists(cert_path) || boost::filesystem::is_directory(cert_path) ||
      !boost::filesystem::exists(pkey_path) || boost::filesystem::is_directory(pkey_path)) {
    return false;
  }
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
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.tls_cacert_path));
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.tls_clientcert_path));
  boost::filesystem::remove(Utils::absolutePath(config_.path, config_.tls_pkey_path));
}

bool FSStorage::loadTlsCommon(std::string* data, const boost::filesystem::path& path_in) {
  boost::filesystem::path path(Utils::absolutePath(config_.path, path_in));
  if (!boost::filesystem::exists(path)) return false;

  if (data) *data = Utils::readFile(path.string());

  return true;
}

bool FSStorage::loadTlsCa(std::string* ca) { return loadTlsCommon(ca, config_.tls_cacert_path); }

bool FSStorage::loadTlsCert(std::string* cert) { return loadTlsCommon(cert, config_.tls_clientcert_path); }

bool FSStorage::loadTlsPkey(std::string* pkey) { return loadTlsCommon(pkey, config_.tls_pkey_path); }

#ifdef BUILD_OSTREE
void FSStorage::storeMetadata(const Uptane::MetaPack& metadata) {
  boost::filesystem::path image_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";

  Utils::writeFile((director_path / "root.json"), metadata.director_root.toJson());
  Utils::writeFile((director_path / "targets.json"), metadata.director_targets.toJson());
  Utils::writeFile((image_path / "root.json"), metadata.image_root.toJson());
  Utils::writeFile((image_path / "targets.json"), metadata.image_targets.toJson());
  Utils::writeFile((image_path / "timestamp.json"), metadata.image_timestamp.toJson());
  Utils::writeFile((image_path / "snapshot.json"), metadata.image_snapshot.toJson());
  sync();
}

bool FSStorage::loadMetadata(Uptane::MetaPack* metadata) {
  boost::filesystem::path image_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";

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

void FSStorage::clearMetadata() {
  boost::filesystem::path image_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config_.path, config_.uptane_metadata_path) / "director";

  boost::filesystem::remove(director_path / "root.json");
  boost::filesystem::remove(director_path / "targets.json");
  boost::filesystem::remove(director_path / "root.json");
  boost::filesystem::remove(director_path / "targets.json");
  boost::filesystem::remove(image_path / "timestamp.json");
  boost::filesystem::remove(image_path / "snapshot.json");
}

#endif  // BUILD_OSTREE

void FSStorage::storeDeviceId(const std::string& device_id) {
  Utils::writeFile(Utils::absolutePath(config_.path, "device_id"), device_id);
}

bool FSStorage::loadDeviceId(std::string* device_id) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "device_id").string())) return false;

  if (device_id) *device_id = Utils::readFile(Utils::absolutePath(config_.path, "device_id").string());
  return true;
}

void FSStorage::clearDeviceId() { boost::filesystem::remove(Utils::absolutePath(config_.path, "device_id")); }

void FSStorage::storeEcuRegistered() {
  Utils::writeFile(Utils::absolutePath(config_.path, "is_registered"), std::string("1"));
}

bool FSStorage::loadEcuRegistered() {
  return boost::filesystem::exists(Utils::absolutePath(config_.path, "is_registered").string());
}

void FSStorage::clearEcuRegistered() { boost::filesystem::remove(Utils::absolutePath(config_.path, "is_registered")); }

void FSStorage::storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) {
  if (serials.size() >= 1) {
    Utils::writeFile(Utils::absolutePath(config_.path, "primary_ecu_serial"), serials[0].first);
    Utils::writeFile(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"), serials[0].second);

    boost::filesystem::remove_all(Utils::absolutePath(config_.path, "secondaries_list"));
    std::vector<std::pair<std::string, std::string> >::const_iterator it;
    std::ofstream file(Utils::absolutePath(config_.path, "secondaries_list").c_str());
    for (it = serials.begin() + 1; it != serials.end(); it++) {
      // Assuming that there are no tabs and linebreaks in serials and hardware ids
      file << it->first << "\t" << it->second << "\n";
    }
    file.close();
  }
}

bool FSStorage::loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) {
  std::string buf;
  std::string serial;
  std::string hw_id;
  if (!boost::filesystem::exists((Utils::absolutePath(config_.path, "primary_ecu_serial")))) {
    return false;
  }
  serial = Utils::readFile(Utils::absolutePath(config_.path, "primary_ecu_serial").string());
  // use default hardware ID for backwards compatibility
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"))) {
    hw_id = Utils::getHostname();
  } else {
    hw_id = Utils::readFile(Utils::absolutePath(config_.path, "primary_ecu_hardware_id").string());
  }

  if (serials) {
    serials->push_back(std::pair<std::string, std::string>(serial, hw_id));
  }

  // return true for backwards compatibility
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "secondaries_list"))) {
    return true;
  }
  std::ifstream file(Utils::absolutePath(config_.path, "secondaries_list").c_str());
  while (std::getline(file, buf)) {
    size_t tab = buf.find('\t');
    serial = buf.substr(0, tab);
    try {
      hw_id = buf.substr(tab + 1);
    } catch (const std::out_of_range& e) {
      if (serials) {
        serials->clear();
      }
      file.close();
      return false;
    }
    if (serials) {
      serials->push_back(std::pair<std::string, std::string>(serial, hw_id));
    }
  }
  file.close();
  return true;
}

void FSStorage::clearEcuSerials() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "primary_ecu_serial"));
  boost::filesystem::remove(Utils::absolutePath(config_.path, "primary_ecu_hardware_id"));
  boost::filesystem::remove(Utils::absolutePath(config_.path, "secondaries_list"));
}

void FSStorage::storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) {
  Json::Value json(Json::arrayValue);
  std::vector<MisconfiguredEcu>::const_iterator it;
  for (it = ecus.begin(); it != ecus.end(); it++) {
    Json::Value ecu;
    ecu["serial"] = it->serial;
    ecu["hardware_id"] = it->hardware_id;
    ecu["state"] = it->state;
    json.append(ecu);
  }
  Utils::writeFile(Utils::absolutePath(config_.path, "misconfigured_ecus"), Json::FastWriter().write(json));
}

bool FSStorage::loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "misconfigured_ecus"))) return false;
  Json::Value content_json = Utils::parseJSONFile(Utils::absolutePath(config_.path, "misconfigured_ecus").string());

  for (Json::ValueIterator it = content_json.begin(); it != content_json.end(); ++it) {
    ecus->push_back(MisconfiguredEcu((*it)["serial"].asString(), (*it)["hardware_id"].asString(),
                                     static_cast<EcuState>((*it)["state"].asInt())));
  }
  return true;
}

void FSStorage::clearMisconfiguredEcus() {
  boost::filesystem::remove(Utils::absolutePath(config_.path, "misconfigured_ecus"));
}

void FSStorage::storeInstalledVersions(const std::map<std::string, std::string>& installed_versions) {
  Json::Value content;
  std::map<std::string, std::string>::const_iterator it;
  for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
    content[it->first] = it->second;
  }

  Utils::writeFile(Utils::absolutePath(config_.path, "installed_versions"), Json::FastWriter().write(content));
}

bool FSStorage::loadInstalledVersions(std::map<std::string, std::string>* installed_versions) {
  if (!boost::filesystem::exists(Utils::absolutePath(config_.path, "installed_versions"))) return false;
  Json::Value content_json = Utils::parseJSONFile(Utils::absolutePath(config_.path, "installed_versions").string());

  for (Json::ValueIterator it = content_json.begin(); it != content_json.end(); ++it) {
    (*installed_versions)[it.key().asString()] = (*it).asString();
  }
  return true;
}

void FSStorage::clearInstalledVersions() {
  if (boost::filesystem::exists(Utils::absolutePath(config_.path, "installed_versions"))) {
    boost::filesystem::remove(Utils::absolutePath(config_.path, "installed_versions"));
  }
}
void FSStorage::cleanUp() {
  boost::filesystem::remove_all(Utils::absolutePath(config_.path, config_.uptane_metadata_path));
}
