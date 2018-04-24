#include "sqlstorage.h"

#include <sys/stat.h>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "logging/logging.h"
#include "sql_utils.h"
#include "utilities/utils.h"

SQLStorage::SQLStorage(const StorageConfig& config) : INvStorage(config) {
  if (!boost::filesystem::is_directory(config.schemas_path)) {
    throw std::runtime_error("Aktualizr installation incorrect. Schemas directory " + config.schemas_path.string() +
                             " missing");
  }

  if (!boost::filesystem::is_directory(config.sqldb_path.parent_path())) {
    Utils::createDirectories(config.sqldb_path.parent_path(), S_IRWXU);
  } else {
    struct stat st {};
    stat(config.sqldb_path.parent_path().c_str(), &st);
    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
      throw std::runtime_error("Storage directory has unsafe permissions");
    }
    if ((st.st_mode & (S_IRGRP | S_IROTH)) != 0) {
      // Remove read permissions for group and others
      chmod(config.sqldb_path.parent_path().c_str(), S_IRWXU);
    }
  }

  if (!dbMigrate()) {
    LOG_ERROR << "SQLite database migration failed";
    // Continue to run anyway, it can't be worse
  } else if (!dbInit()) {
    LOG_ERROR << "Couldn't initialize database";
  }
}

void SQLStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM primary_keys;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of keys table: " << db.errmsg();
    return;
  }
  const char* req;
  if (std::stoi(req_response["result"]) != 0) {
    req = "UPDATE OR REPLACE primary_keys SET (public, private) = (?,?);";
  } else {
    req = "INSERT INTO primary_keys(public,private) VALUES (?,?);";
  }
  auto statement = db.prepareStatement<std::string>(req, public_key, private_key);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set primary keys: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) {
  return loadPrimaryPublic(public_key) && loadPrimaryPrivate(private_key);
}

bool SQLStorage::loadPrimaryPublic(std::string* public_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT public FROM primary_keys LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get public key: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) {
    return false;
  }

  if (public_key != nullptr) {
    *public_key = req_response["result"];
  }

  return true;
}

bool SQLStorage::loadPrimaryPrivate(std::string* private_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT private FROM primary_keys LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get private key: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) {
    return false;
  }

  if (private_key != nullptr) {
    *private_key = req_response["result"];
  }

  return true;
}

void SQLStorage::clearPrimaryKeys() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM primary_keys;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear primary keys: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  storeTlsCa(ca);
  storeTlsCert(cert);
  storeTlsPkey(pkey);
}

void SQLStorage::storeTlsCa(const std::string& ca) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM tls_creds;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of tls_creds table: " << db.errmsg();
    return;
  }
  const char* req;
  if (std::stoi(req_response["result"]) != 0) {
    req = "UPDATE OR REPLACE tls_creds SET ca_cert = ?;";
  } else {
    req = "INSERT INTO tls_creds(ca_cert) VALUES (?);";
  }
  auto statement = db.prepareStatement<SQLBlob>(req, SQLBlob(ca));
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set ca_cert: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeTlsCert(const std::string& cert) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM tls_creds;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of tls_creds table: " << db.errmsg();
    return;
  }
  const char* req;
  if (std::stoi(req_response["result"]) != 0) {
    req = "UPDATE OR REPLACE tls_creds SET client_cert = ?;";
  } else {
    req = "INSERT INTO tls_creds(client_cert) VALUES (?);";
  }
  auto statement = db.prepareStatement<SQLBlob>(req, SQLBlob(cert));
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set client_cert: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeTlsPkey(const std::string& pkey) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM tls_creds;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of tls_creds table: " << db.errmsg();
    return;
  }
  const char* req;
  if (std::stoi(req_response["result"]) != 0) {
    req = "UPDATE OR REPLACE tls_creds SET client_pkey = ?;";
  } else {
    req = "INSERT INTO tls_creds(client_pkey) VALUES (?);";
  }
  auto statement = db.prepareStatement<SQLBlob>(req, SQLBlob(pkey));
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set client_pkey: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (db.exec("SELECT * FROM tls_creds LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get tls_creds: " << db.errmsg();
    return false;
  }

  if (req_response_table.empty()) {
    return false;
  }

  if (ca != nullptr) {
    *ca = req_response_table[0]["ca_cert"];
  }
  if (cert != nullptr) {
    *cert = req_response_table[0]["client_cert"];
  }
  if (pkey != nullptr) {
    *pkey = req_response_table[0]["client_pkey"];
  }
  return true;
}

void SQLStorage::clearTlsCreds() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM tls_creds;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear tls_creds: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadTlsCa(std::string* ca) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT ca_cert FROM tls_creds LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) {
    return false;
  }

  if (ca != nullptr) {
    *ca = req_response["result"];
  }

  return true;
}

bool SQLStorage::loadTlsCert(std::string* cert) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT client_cert FROM tls_creds LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) {
    return false;
  }

  if (cert != nullptr) {
    *cert = req_response["result"];
  }

  return true;
}

bool SQLStorage::loadTlsPkey(std::string* pkey) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT client_pkey FROM tls_creds LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get client_pkey: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) {
    return false;
  }

  if (pkey != nullptr) {
    *pkey = req_response["result"];
  }

  return true;
}

void SQLStorage::storeMetadata(const Uptane::MetaPack& metadata) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (!db.beginTransaction()) {
    return;
  }

  if (db.exec("DELETE FROM meta;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear meta: " << db.errmsg();
    return;
  }

  std::vector<std::string> jsons;
  jsons.push_back(Json::FastWriter().write(metadata.director_root.original()));
  jsons.push_back(Json::FastWriter().write(metadata.director_targets.original()));
  jsons.push_back(Json::FastWriter().write(metadata.image_root.original()));
  jsons.push_back(Json::FastWriter().write(metadata.image_targets.original()));
  jsons.push_back(Json::FastWriter().write(metadata.image_timestamp.original()));
  jsons.push_back(Json::FastWriter().write(metadata.image_snapshot.original()));

  auto statement = db.prepareStatement<SQLBlob, SQLBlob, SQLBlob, SQLBlob, SQLBlob, SQLBlob>(
      "INSERT INTO meta VALUES (?,?,?,?,?,?);", SQLBlob(jsons[0]), SQLBlob(jsons[1]), SQLBlob(jsons[2]),
      SQLBlob(jsons[3]), SQLBlob(jsons[4]), SQLBlob(jsons[5]));

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set metadata: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadMetadata(Uptane::MetaPack* metadata) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (db.exec("SELECT * FROM meta;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get meta: " << db.errmsg();
    return false;
  }
  if (req_response_table.empty()) {
    return false;
  }

  Json::Value json = Utils::parseJSON(req_response_table[0]["director_root"]);
  if (json != Json::nullValue) {
    Json::Value fixed_json = json;
    if (!json.isMember("signed")) {
      fixed_json["signed"] = json;  // Old format contains only signed part.
    }
    metadata->director_root = Uptane::Root("director", fixed_json);
  } else {
    return false;
  }

  json = Utils::parseJSON(req_response_table[0]["image_root"]);
  if (json != Json::nullValue) {
    Json::Value fixed_json = json;
    if (!json.isMember("signed")) {
      fixed_json["signed"] = json;
    }
    metadata->image_root = Uptane::Root("image", fixed_json);
  } else {
    return false;
  }

  json = Utils::parseJSON(req_response_table[0]["director_targets"]);
  if (json != Json::nullValue) {
    Json::Value fixed_json = json;
    if (!json.isMember("signed")) {
      fixed_json["signed"] = json;
    }
    metadata->director_targets = Uptane::Targets(fixed_json);
  }

  json = Utils::parseJSON(req_response_table[0]["image_targets"]);
  if (json != Json::nullValue) {
    Json::Value fixed_json = json;
    if (!json.isMember("signed")) {
      fixed_json["signed"] = json;
    }
    metadata->image_targets = Uptane::Targets(fixed_json);
  }

  json = Utils::parseJSON(req_response_table[0]["image_timestamp"]);
  if (json != Json::nullValue) {
    Json::Value fixed_json = json;
    if (!json.isMember("signed")) {
      fixed_json["signed"] = json;
    }
    metadata->image_timestamp = Uptane::TimestampMeta(fixed_json);
  }

  json = Utils::parseJSON(req_response_table[0]["image_snapshot"]);
  if (json != Json::nullValue) {
    Json::Value fixed_json = json;
    if (!json.isMember("signed")) {
      fixed_json["signed"] = json;
    }
    metadata->image_snapshot = Uptane::Snapshot(fixed_json);
  }

  return true;
}

void SQLStorage::clearMetadata() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM meta;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear meta: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeDeviceId(const std::string& device_id) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  auto statement = db.prepareStatement<std::string>("UPDATE OR REPLACE device_info SET device_id = ?;", device_id);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set device ID: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadDeviceId(std::string* device_id) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT device_id FROM device_info LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) {
    return false;
  }

  if (device_id != nullptr) {
    *device_id = req_response["result"];
  }

  return true;
}

void SQLStorage::clearDeviceId() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("UPDATE OR REPLACE device_info SET device_id = NULL;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear device ID: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeEcuRegistered() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  std::string req = "UPDATE OR REPLACE device_info SET is_registered = 1";
  if (db.exec(req.c_str(), nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadEcuRegistered() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT is_registered FROM device_info LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) {
    return false;
  }

  return std::stoi(req_response["result"]) != 0;
}

void SQLStorage::clearEcuRegistered() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  std::string req = "UPDATE OR REPLACE device_info SET is_registered = 0";
  if (db.exec(req.c_str(), nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) {
  if (serials.size() >= 1) {
    SQLite3Guard db(config_.sqldb_path.c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db.errmsg();
      return;
    }

    if (!db.beginTransaction()) {
      return;
    }

    if (db.exec("DELETE FROM ecu_serials;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't clear ecu_serials: " << db.errmsg();
      return;
    }

    std::string serial = serials[0].first, hwid = serials[0].second;
    auto statement =
        db.prepareStatement<std::string, std::string>("INSERT INTO ecu_serials VALUES (?,?,1);", serial, hwid);
    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
      return;
    }

    std::vector<std::pair<std::string, std::string> >::const_iterator it;
    for (it = serials.begin() + 1; it != serials.end(); it++) {
      auto statement = db.prepareStatement<std::string, std::string>("INSERT INTO ecu_serials VALUES (?,?,0);",
                                                                     it->first, it->second);

      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
        return;
      }
    }

    db.commitTransaction();
  }
}

bool SQLStorage::loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (db.exec("SELECT * FROM ecu_serials ORDER BY is_primary DESC;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get ecu_serials: " << db.errmsg();
    return false;
  }
  if (req_response_table.empty()) {
    return false;
  }

  std::vector<std::map<std::string, std::string> >::iterator it;
  for (it = req_response_table.begin(); it != req_response_table.end(); ++it) {
    serials->push_back(std::make_pair((*it)["serial"], (*it)["hardware_id"]));
  }

  return true;
}

void SQLStorage::clearEcuSerials() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM ecu_serials;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear ecu_serials: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) {
  if (ecus.size() >= 1) {
    SQLite3Guard db(config_.sqldb_path.c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db.errmsg();
      return;
    }

    if (!db.beginTransaction()) {
      return;
    }

    if (db.exec("DELETE FROM misconfigured_ecus;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't clear misconfigured_ecus: " << db.errmsg();
      return;
    }

    std::vector<MisconfiguredEcu>::const_iterator it;
    for (it = ecus.begin(); it != ecus.end(); it++) {
      auto statement = db.prepareStatement<std::string, std::string, int>(
          "INSERT INTO misconfigured_ecus VALUES (?,?,?);", it->serial, it->hardware_id, it->state);

      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set misconfigured_ecus: " << db.errmsg();
        return;
      }
    }

    db.commitTransaction();
  }
}

bool SQLStorage::loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (db.exec("SELECT * FROM misconfigured_ecus;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get misconfigured_ecus: " << db.errmsg();
    return false;
  }
  if (req_response_table.empty()) {
    return false;
  }

  std::vector<std::map<std::string, std::string> >::iterator it;
  for (it = req_response_table.begin(); it != req_response_table.end(); ++it) {
    ecus->push_back(
        MisconfiguredEcu((*it)["serial"], (*it)["hardware_id"], static_cast<EcuState>(std::stoi((*it)["state"]))));
  }

  return true;
}

void SQLStorage::clearMisconfiguredEcus() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM misconfigured_ecus;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear misconfigured_ecus: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeInstalledVersions(const std::vector<Uptane::Target>& installed_versions,
                                        const std::string& current_hash) {
  if (installed_versions.size() >= 1) {
    SQLite3Guard db(config_.sqldb_path.c_str());

    if (!db.beginTransaction()) {
      return;
    }

    if (db.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db.errmsg();
      return;
    }

    if (db.exec("DELETE FROM installed_versions;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't clear installed_versions: " << db.errmsg();
      return;
    }

    std::vector<Uptane::Target>::const_iterator it;
    for (it = installed_versions.cbegin(); it != installed_versions.cend(); it++) {
      std::string sql = "INSERT INTO installed_versions VALUES (?,?,?,?);";
      std::string hash = it->sha256Hash();
      std::string filename = it->filename();
      bool is_current = current_hash == it->sha256Hash();
      int64_t size = it->length();
      auto statement = db.prepareStatement<std::string, std::string, int, int>(
          sql, hash, filename, static_cast<const int>(is_current), size);

      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set installed_versions: " << db.errmsg();
        return;
      }
    }

    db.commitTransaction();
  }
}

std::string SQLStorage::loadInstalledVersions(std::vector<Uptane::Target>* installed_versions) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  std::string current_hash;
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return current_hash;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (db.exec("SELECT * FROM installed_versions;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get installed_versions: " << db.errmsg();
    return current_hash;
  }
  if (req_response_table.empty()) {
    return current_hash;
  }

  std::vector<std::map<std::string, std::string> >::iterator it;
  for (it = req_response_table.begin(); it != req_response_table.end(); ++it) {
    Json::Value installed_version;
    installed_version["hashes"]["sha256"] = (*it)["hash"];
    installed_version["length"] = Json::UInt64(std::stoll((*it)["length"]));
    if (std::stoi((*it)["is_current"]) != 0) {
      current_hash = (*it)["hash"];
    }
    std::string filename = (*it)["name"];
    Uptane::Target target(filename, installed_version);
    installed_versions->push_back(target);
  }

  return current_hash;
}

void SQLStorage::clearInstalledVersions() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM installed_versions;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear installed_versions: " << db.errmsg();
    return;
  }
}

class SQLTargetWHandle : public StorageTargetWHandle {
 public:
  SQLTargetWHandle(const SQLStorage& storage, std::string filename, size_t size)
      : db_(storage.config_.sqldb_path.c_str()),
        filename_(std::move(filename)),
        expected_size_(size),
        written_size_(0),
        closed_(false),
        blob_(nullptr) {
    StorageTargetWHandle::WriteError exc("could not save file " + filename_ + " to sql storage");

    if (db_.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db_.errmsg();
      throw exc;
    }

    // allocate a zero blob
    if (!db_.beginTransaction()) {
      throw exc;
    }

    auto statement = db_.prepareStatement<std::string, SQLZeroBlob>(
        "INSERT OR REPLACE INTO target_images (filename, image_data) VALUES (?, ?);", filename_,
        SQLZeroBlob{expected_size_});

    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Statement step failure: " << db_.errmsg();
      throw exc;
    }

    // open the created blob for writing
    sqlite3_int64 row_id = sqlite3_last_insert_rowid(db_.get());

    if (sqlite3_blob_open(db_.get(), "main", "target_images", "image_data", row_id, 1, &blob_) != SQLITE_OK) {
      LOG_ERROR << "Could not open blob " << db_.errmsg();
      throw exc;
    }
  }

  ~SQLTargetWHandle() override {
    if (!closed_) {
      LOG_WARNING << "Handle for file " << filename_ << " has not been committed or aborted, forcing abort";
      SQLTargetWHandle::wabort();
    }
  }

  size_t wfeed(const uint8_t* buf, size_t size) override {
    if (sqlite3_blob_write(blob_, buf, static_cast<int>(size), written_size_) != SQLITE_OK) {
      LOG_ERROR << "Could not write in blob: " << db_.errmsg();
      return 0;
    }
    written_size_ += size;
    return size;
  }

  void wcommit() override {
    closed_ = true;
    sqlite3_blob_close(blob_);
    blob_ = nullptr;
    if (!db_.commitTransaction()) {
      throw StorageTargetWHandle::WriteError("could not save file " + filename_ + " to sql storage");
    }
  }

  void wabort() noexcept override {
    closed_ = true;
    if (blob_ != nullptr) {
      sqlite3_blob_close(blob_);
      blob_ = nullptr;
    }

    db_.rollbackTransaction();
  }

 private:
  SQLite3Guard db_;
  const std::string filename_;
  size_t expected_size_;
  size_t written_size_;
  bool closed_;
  sqlite3_blob* blob_;
};

std::unique_ptr<StorageTargetWHandle> SQLStorage::allocateTargetFile(bool from_director, const std::string& filename,
                                                                     size_t size) {
  (void)from_director;

  return std::unique_ptr<StorageTargetWHandle>(new SQLTargetWHandle(*this, filename, size));
}

class SQLTargetRHandle : public StorageTargetRHandle {
 public:
  SQLTargetRHandle(const SQLStorage& storage, const std::string& filename)
      : db_(storage.config_.sqldb_path.c_str()),
        filename_(filename),
        size_(0),
        read_size_(0),
        closed_(false),
        blob_(nullptr) {
    StorageTargetRHandle::ReadError exc("could not read file " + filename_ + " from sql storage");

    if (db_.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db_.errmsg();
      throw exc;
    }

    if (!db_.beginTransaction()) {
      throw exc;
    }

    auto statement = db_.prepareStatement<std::string>("SELECT rowid FROM target_images WHERE filename = ?;", filename);

    int err = statement.step();
    if (err == SQLITE_DONE) {
      LOG_ERROR << "No such file in db: " + filename_;
      throw exc;
    }
    if (err != SQLITE_ROW) {
      LOG_ERROR << "Statement step failure: " << db_.errmsg();
      throw exc;
    }

    sqlite3_int64 row_id = sqlite3_column_int64(statement.get(), 0);

    if (sqlite3_blob_open(db_.get(), "main", "target_images", "image_data", row_id, 0, &blob_) != SQLITE_OK) {
      LOG_ERROR << "Could not open blob: " << db_.errmsg();
      throw exc;
    }
    size_ = sqlite3_blob_bytes(blob_);

    if (!db_.commitTransaction()) {
      throw exc;
    }
  }

  ~SQLTargetRHandle() override {
    if (!closed_) {
      SQLTargetRHandle::rclose();
    }
  }

  size_t rsize() const override { return size_; }

  size_t rread(uint8_t* buf, size_t size) override {
    if (read_size_ + size > size_) {
      size = size_ - read_size_;
    }
    if (size == 0) {
      return 0;
    }

    if (sqlite3_blob_read(blob_, buf, static_cast<int>(size), read_size_) != SQLITE_OK) {
      LOG_ERROR << "Could not read from blob: " << db_.errmsg();
      return 0;
    }
    read_size_ += size;

    return size;
  }

  void rclose() noexcept override {
    sqlite3_blob_close(blob_);
    closed_ = true;
  }

 private:
  SQLite3Guard db_;
  const std::string filename_;
  size_t size_;
  size_t read_size_;
  bool closed_;
  sqlite3_blob* blob_;
};

std::unique_ptr<StorageTargetRHandle> SQLStorage::openTargetFile(const std::string& filename) {
  return std::unique_ptr<StorageTargetRHandle>(new SQLTargetRHandle(*this, filename));
}

void SQLStorage::removeTargetFile(const std::string& filename) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  auto statement = db.prepareStatement<std::string>("DELETE FROM target_images WHERE filename=?;", filename);

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Statement step failure: " << db.errmsg();
    throw std::runtime_error("Could not remove target file");
  }

  if (sqlite3_changes(db.get()) != 1) {
    throw std::runtime_error("Target file " + filename + " not found");
  }
}

void SQLStorage::cleanUp() { boost::filesystem::remove_all(config_.sqldb_path); }

std::string SQLStorage::getTableSchemaFromDb(const std::string& tablename) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return "";
  }

  auto statement = db.prepareStatement<std::string>(
      "SELECT sql FROM sqlite_master WHERE type='table' AND tbl_name=? LIMIT 1;", tablename);

  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get schema of " << tablename << ": " << db.errmsg();
    return "";
  }

  return std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0))) + ";";
}

bool SQLStorage::dbMigrate() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  int schema_version = getVersion();
  if (schema_version == kSqlSchemaVersion) {
    return true;
  } else if (schema_version == DbState::kInvalid) {
    LOG_ERROR << "We point to the wrong sqlite database file.";
    return false;
  }

  for (; schema_version < kSqlSchemaVersion; schema_version++) {
    boost::filesystem::path migrate_script_path =
        config_.schemas_path / (std::string("migrate.") + std::to_string(schema_version + 1) + ".sql");
    std::string req = Utils::readFile(migrate_script_path.string());

    if (db.exec(req.c_str(), nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't migrate db from version" << (schema_version - 1) << " to version " << schema_version << ": "
                << db.errmsg();
      return false;
    }
  }

  return true;
}

bool SQLStorage::dbInit() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (!db.beginTransaction()) {
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM device_info;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get number of rows in device_info: " << db.errmsg();
    return false;
  }

  if (std::stoi(req_response["result"]) < 1) {
    if (db.exec("INSERT INTO device_info DEFAULT VALUES;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't set default values to device_info: " << db.errmsg();
      return false;
    }
  }

  return db.commitTransaction();
}

int SQLStorage::getVersion() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return DbState::kInvalid;
  }

  request = kSqlGetSimple;
  req_response.clear();
  std::string req = std::string("SELECT count(*) FROM sqlite_master WHERE type='table';");
  if (db.exec(req.c_str(), callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get tables count: " << db.errmsg();
    return DbState::kInvalid;
  }
  if (std::stoi(req_response["result"]) == 0) {
    return DbState::kEmpty;
  }

  req_response.clear();
  req = std::string("SELECT version FROM version LIMIT 1;");
  if (db.exec(req.c_str(), callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get database version: " << db.errmsg();
    return DbState::kInvalid;
  }

  try {
    return std::stoi(req_response["result"]);
  } catch (const std::exception&) {
    return DbState::kInvalid;
  }
}

int SQLStorage::callback(void* instance_, int numcolumns, char** values, char** columns) {
  auto* instance = static_cast<SQLStorage*>(instance_);

  switch (instance->request) {
    case kSqlGetSimple: {
      (void)numcolumns;
      (void)columns;
      if (values[0] != nullptr) {
        instance->req_response["result"] = values[0];
      }
      break;
    }
    case kSqlGetTable: {
      std::map<std::string, std::string> row;
      for (int i = 0; i < numcolumns; ++i) {
        if (values[i] != nullptr) {
          row[columns[i]] = values[i];
        }
      }
      instance->req_response_table.push_back(row);
      break;
    }
    default:
      return -1;
  }
  return 0;
}
