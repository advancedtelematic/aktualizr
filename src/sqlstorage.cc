#include "sqlstorage.h"

#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/scoped_array.hpp>

#include "logger.h"
#include "utils.h"

SQLStorage::SQLStorage(const Config& config) : config_(config) {
  // FSStorage
  boost::filesystem::create_directories(config_.tls.certificates_directory);
  boost::filesystem::create_directories(config_.uptane.metadata_path / "repo");
  boost::filesystem::create_directories(config_.uptane.metadata_path / "director");

  // SQLStorage
}

SQLStorage::~SQLStorage() {
  // TODO: clear director_files, image_files
}

void SQLStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  boost::filesystem::path public_key_path = config_.tls.certificates_directory / config_.uptane.public_key_path;
  boost::filesystem::path private_key_path = config_.tls.certificates_directory / config_.uptane.private_key_path;

  boost::filesystem::remove(public_key_path);
  boost::filesystem::remove(private_key_path);

  Utils::writeFile(public_key_path.string(), public_key);
  Utils::writeFile(private_key_path.string(), private_key);

  sync();
}

bool SQLStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) {
  boost::filesystem::path public_key_path = config_.tls.certificates_directory / config_.uptane.public_key_path;
  boost::filesystem::path private_key_path = config_.tls.certificates_directory / config_.uptane.private_key_path;

  if (!boost::filesystem::exists(public_key_path) || !boost::filesystem::exists(private_key_path)) {
    return false;
  }

  if (public_key) *public_key = Utils::readFile(public_key_path.string());
  if (private_key) *private_key = Utils::readFile(private_key_path.string());
  return true;
}

void SQLStorage::clearPrimaryKeys() {
  boost::filesystem::remove(config_.tls.certificates_directory / config_.uptane.public_key_path);
  boost::filesystem::remove(config_.tls.certificates_directory / config_.uptane.private_key_path);
}

void SQLStorage::storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  if (config_.tls.ca_source == kFile) {
    boost::filesystem::path ca_path(config_.tls.ca_file());
    boost::filesystem::remove(ca_path);
    Utils::writeFile(ca_path.string(), ca);
  }

  if (config_.tls.cert_source == kFile) {
    boost::filesystem::path cert_path(config_.tls.client_certificate());
    boost::filesystem::remove(cert_path);
    Utils::writeFile(cert_path.string(), cert);
  }

  if (config_.tls.pkey_source == kFile) {
    boost::filesystem::path pkey_path(config_.tls.pkey_file());
    boost::filesystem::remove(pkey_path);
    Utils::writeFile(pkey_path.string(), pkey);
  }

  sync();
}

bool SQLStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  boost::filesystem::path ca_path(config_.tls.ca_file());
  boost::filesystem::path cert_path(config_.tls.client_certificate());
  boost::filesystem::path pkey_path(config_.tls.pkey_file());
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

void SQLStorage::clearTlsCreds() {
  boost::filesystem::remove(config_.tls.ca_file());
  boost::filesystem::remove(config_.tls.client_certificate());
  boost::filesystem::remove(config_.tls.pkey_file());
}

bool SQLStorage::loadTlsCommon(std::string* data, const std::string& path_in) {
  boost::filesystem::path path(path_in);
  if (!boost::filesystem::exists(path)) return false;

  if (data) *data = Utils::readFile(path.string());

  return true;
}

bool SQLStorage::loadTlsCa(std::string* ca) { return loadTlsCommon(ca, config_.tls.ca_file()); }

bool SQLStorage::loadTlsCert(std::string* cert) { return loadTlsCommon(cert, config_.tls.client_certificate()); }

bool SQLStorage::loadTlsPkey(std::string* pkey) { return loadTlsCommon(pkey, config_.tls.pkey_file()); }

#ifdef BUILD_OSTREE
void SQLStorage::storeMetadata(const Uptane::MetaPack& metadata) {
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

bool SQLStorage::loadMetadata(Uptane::MetaPack* metadata) {
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

void SQLStorage::storeDeviceId(const std::string& device_id) {
  Utils::writeFile((config_.tls.certificates_directory / "device_id").string(), device_id);
}

bool SQLStorage::loadDeviceId(std::string* device_id) {
  if (!boost::filesystem::exists((config_.tls.certificates_directory / "device_id").string())) return false;

  if (device_id) *device_id = Utils::readFile((config_.tls.certificates_directory / "device_id").string());
  return true;
}

void SQLStorage::clearDeviceId() { boost::filesystem::remove(config_.tls.certificates_directory / "device_id"); }

void SQLStorage::storeEcuRegistered() {
  Utils::writeFile((config_.tls.certificates_directory / "is_registered").string(), std::string("1"));
}

bool SQLStorage::loadEcuRegistered() {
  return boost::filesystem::exists((config_.tls.certificates_directory / "is_registered").string());
}

void SQLStorage::clearEcuRegistered() {
  boost::filesystem::remove(config_.tls.certificates_directory / "is_registered");
}

void SQLStorage::storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) {
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

bool SQLStorage::loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) {
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

void SQLStorage::clearEcuSerials() {
  boost::filesystem::remove(config_.tls.certificates_directory / "primary_ecu_serial");
  boost::filesystem::remove(config_.tls.certificates_directory / "primary_hardware_id");
  boost::filesystem::remove(config_.tls.certificates_directory / "secondaries_list");
}

void SQLStorage::storeInstalledVersions(const std::string& content) {
  Utils::writeFile((config_.tls.certificates_directory / "installed_versions").string(), content);
}

bool SQLStorage::loadInstalledVersions(std::string* content) {
  if (!boost::filesystem::exists(config_.tls.certificates_directory / "installed_versions")) return false;
  *content = Utils::readFile((config_.tls.certificates_directory / "installed_versions").string());
  return true;
}

bool SQLStorage::createFromSchema(boost::filesystem::path db_path, const Json::Value& schema) {
  try {
    SQLite3Guard db(db_path.string().c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOGGER_LOG(LVL_error, "Can't open database: " << sqlite3_errmsg(db.get()));
      return false;
    }

    if (sqlite3_exec(db.get(), "BEGIN TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK) {
      LOGGER_LOG(LVL_error, "Can't open database: " << sqlite3_errmsg(db.get()));
      return false;
    }

    for (Json::ValueIterator it = schema.begin(); it != schema.end(); it++) {
      // "_version" field is metadata and doesn't describe a table
      if (it.key().asString() == "_version") continue;

      std::string create_table = "CREATE TABLE " + it.key().asString() + "(";
      for (Json::ValueIterator t_it = (*it).begin(); t_it != (*it).end(); t_it++)
        create_table += t_it.key().asString() + " " + (*t_it).asString() + ",";
      create_table.erase(create_table.end() - 1);  // erase last comma (no pop_back before c++11)
      create_table.push_back(')');
      create_table.push_back(';');

      if (sqlite3_exec(db.get(), create_table.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
        LOGGER_LOG(LVL_error, "Can't create table: " << sqlite3_errmsg(db.get()));
        return false;
      }
    }
    if (sqlite3_exec(db.get(), "COMMIT TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK) {
      LOGGER_LOG(LVL_error, "Can't open database: " << sqlite3_errmsg(db.get()));
      return false;
    }
  } catch (std::exception e) {
    LOGGER_LOG(LVL_error, "Exception while creating an sqlite3 db" << e.what());
    return false;
  }
  return true;
}

bool SQLStorage::checkSchema(boost::filesystem::path db_path, const Json::Value& schema) {
  try {
    SQLite3Guard db(db_path.string().c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOGGER_LOG(LVL_error, "Can't open database: " << sqlite3_errmsg(db.get()));
      return false;
    }

    for (Json::ValueIterator it = schema.begin(); it != schema.end(); it++) {
      // "_version" field in metadata and doesn't describe a table
      if (it.key().asString() == "_version") continue;

      std::string req = "PRAGMA table_info(";
      req += it.key().asString();
      req.push_back(')');
      req.push_back(';');
      req_response.clear();
      request = kSqlTableInfo;
      if (sqlite3_exec(db.get(), req.c_str(), callback, this, NULL) != SQLITE_OK) {
        LOGGER_LOG(LVL_error, "Can't get table info on " << it.key().asString() << ": " << sqlite3_errmsg(db.get()));
        return false;
      }

      for (Json::ValueIterator t_it = (*it).begin(); t_it != (*it).end(); t_it++) {
        std::string type_resp = req_response[t_it.key().asString()];
        // only what's before first ' ' is a type, anything else are modifiers.
        std::string type_schema = (*t_it).asString().substr(0, (*t_it).asString().find_first_of(' '));
        if (type_resp != type_schema) return false;
      }
    }
  } catch (std::exception e) {
    LOGGER_LOG(LVL_error, "Exception while checking an sqlite3 db " << e.what());
    return false;
  }
  return true;
}

int SQLStorage::callback(void* instance_, int numcolumns, char** values, char** columns) {
  SQLStorage* instance = static_cast<SQLStorage*>(instance_);

  switch (instance->request) {
    case kSqlTableInfo: {
      std::string name;
      std::string type;
      for (int i = 0; i < numcolumns; i++) {
        if (!strcmp(columns[i], "name")) name = values[i];
        if (!strcmp(columns[i], "type")) type = values[i];
      }
      instance->req_response[name] = type;
      break;
    }
    case kSqlGetVersion:
      break;
    default:
      return -1;
  }
  return 0;
}
