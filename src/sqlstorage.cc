#include "sqlstorage.h"

#include <iostream>
#include <memory>

#include <boost/make_shared.hpp>
#include <boost/move/make_unique.hpp>
#include <boost/scoped_array.hpp>

#include "logging.h"
#include "utils.h"

typedef std::unique_ptr<sqlite3_stmt, int (*)(sqlite3_stmt*)> SQLGuardedStatement;

static SQLGuardedStatement sqlite3_prepare_guarded(sqlite3* db, const char* zSql, int nByte) {
  sqlite3_stmt* statement;

  if (sqlite3_prepare_v2(db, zSql, nByte, &statement, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Could not prepare statement: " << sqlite3_errmsg(db);
    throw std::runtime_error("SQLite fatal error");
  }

  return std::unique_ptr<sqlite3_stmt, int (*)(sqlite3_stmt*)>(statement, sqlite3_finalize);
}

SQLStorage::SQLStorage(const StorageConfig& config) : config_(config) {
  if (!boost::filesystem::is_directory(config.schemas_path)) {
    throw std::runtime_error("Aktualizr installation incorrect. Schemas directory " + config.schemas_path.string() +
                             " missing");
  }
  if (!dbMigrate()) {
    LOG_ERROR << "SQLite database migration failed";
    // Continue to run anyway, it can't be worse
  } else if (!dbCheck()) {
    LOG_ERROR << "SQLite database doesn't match its schema";
  } else if (!dbInit()) {
    LOG_ERROR << "Couldn't initialize database";
  }
}

SQLStorage::~SQLStorage() {
  // TODO: clear director_files, image_files
}

void SQLStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  storePrimaryPublic(public_key);
  storePrimaryPrivate(private_key);
}

void SQLStorage::storePrimaryPublic(const std::string& public_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT count(*) FROM primary_keys;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of public key table: " << sqlite3_errmsg(db.get());
    return;
  }
  std::string req;
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE primary_keys SET public = '";
    req += public_key + "';";
  } else {
    req = "INSERT INTO primary_keys(public) VALUES  ('";
    req += public_key + "');";
  }
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set public key: " << sqlite3_errmsg(db.get());
    return;
  }

  sync();
}

void SQLStorage::storePrimaryPrivate(const std::string& private_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT count(*) FROM primary_keys;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of private key table: " << sqlite3_errmsg(db.get());
    return;
  }
  std::string req;
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE primary_keys SET private = '";
    req += private_key + "';";
  } else {
    req = "INSERT INTO primary_keys(private) VALUES  ('";
    req += private_key + "');";
  }
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set private key: " << sqlite3_errmsg(db.get());
    return;
  }

  sync();
}

bool SQLStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) {
  return loadPrimaryPublic(public_key) && loadPrimaryPrivate(private_key);
}

bool SQLStorage::loadPrimaryPublic(std::string* public_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT public FROM primary_keys LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get public key: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  if (public_key) *public_key = req_response["result"];

  return true;
}

bool SQLStorage::loadPrimaryPrivate(std::string* private_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT private FROM primary_keys LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get private key: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  if (private_key) *private_key = req_response["result"];

  return true;
}

void SQLStorage::clearPrimaryKeys() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  if (sqlite3_exec(db.get(), "DELETE FROM primary_keys;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear primary keys: " << sqlite3_errmsg(db.get());
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
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT count(*) FROM tls_creds;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of tls_creds table: " << sqlite3_errmsg(db.get());
    return;
  }
  std::string req;
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE tls_creds SET ca_cert = '";
    req += ca + "';";
  } else {
    req = "INSERT INTO tls_creds(ca_cert) VALUES  ('";
    req += ca + "');";
  }
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set ca_cert: " << sqlite3_errmsg(db.get());
    return;
  }

  sync();
}

void SQLStorage::storeTlsCert(const std::string& cert) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT count(*) FROM tls_creds;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of tls_creds table: " << sqlite3_errmsg(db.get());
    return;
  }
  std::string req;
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE tls_creds SET client_cert = '";
    req += cert + "';";
  } else {
    req = "INSERT INTO tls_creds(client_cert) VALUES  ('";
    req += cert + "');";
  }
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set client_cert: " << sqlite3_errmsg(db.get());
    return;
  }

  sync();
}

void SQLStorage::storeTlsPkey(const std::string& pkey) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT count(*) FROM tls_creds;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of tls_creds table: " << sqlite3_errmsg(db.get());
    return;
  }
  std::string req;
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE tls_creds SET client_pkey = '";
    req += pkey + "';";
  } else {
    req = "INSERT INTO tls_creds(client_pkey) VALUES  ('";
    req += pkey + "');";
  }
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set client_pkey: " << sqlite3_errmsg(db.get());
    return;
  }

  sync();
}

bool SQLStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (sqlite3_exec(db.get(), "SELECT * FROM tls_creds LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get tls_creds: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response_table.empty()) return false;

  if (ca) {
    *ca = req_response_table[0]["ca_cert"];
  }
  if (cert) {
    *cert = req_response_table[0]["client_cert"];
  }
  if (pkey) {
    *pkey = req_response_table[0]["client_pkey"];
  }
  return true;
}

void SQLStorage::clearTlsCreds() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  if (sqlite3_exec(db.get(), "DELETE FROM tls_creds;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear tls_creds: " << sqlite3_errmsg(db.get());
    return;
  }
}

bool SQLStorage::loadTlsCa(std::string* ca) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT ca_cert FROM tls_creds LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  if (ca) *ca = req_response["result"];

  return true;
}

bool SQLStorage::loadTlsCert(std::string* cert) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT client_cert FROM tls_creds LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  if (cert) *cert = req_response["result"];

  return true;
}

bool SQLStorage::loadTlsPkey(std::string* pkey) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT client_pkey FROM tls_creds LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get client_pkey: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  if (pkey) *pkey = req_response["result"];

  return true;
}

void SQLStorage::storeMetadata(const Uptane::MetaPack& metadata) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  clearMetadata();

  std::string req = "INSERT INTO meta VALUES  ('";
  req += Json::FastWriter().write(metadata.director_root.toJson()) + "', '";
  req += Json::FastWriter().write(metadata.director_targets.toJson()) + "', '";
  req += Json::FastWriter().write(metadata.image_root.toJson()) + "', '";
  req += Json::FastWriter().write(metadata.image_targets.toJson()) + "', '";
  req += Json::FastWriter().write(metadata.image_timestamp.toJson()) + "', '";
  req += Json::FastWriter().write(metadata.image_snapshot.toJson()) + "');";

  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set metadata: " << sqlite3_errmsg(db.get());
    return;
  }

  sync();
}

bool SQLStorage::loadMetadata(Uptane::MetaPack* metadata) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (sqlite3_exec(db.get(), "SELECT * FROM meta;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get meta: " << sqlite3_errmsg(db.get());
    return false;
  }
  if (req_response_table.empty()) return false;

  Json::Value json = Utils::parseJSON(req_response_table[0]["director_root"]);
  metadata->director_root = Uptane::Root("director", json);
  json = Utils::parseJSON(req_response_table[0]["director_targets"]);
  metadata->director_targets = Uptane::Targets(json);
  json = Utils::parseJSON(req_response_table[0]["image_root"]);
  metadata->image_root = Uptane::Root("image", json);
  json = Utils::parseJSON(req_response_table[0]["image_targets"]);
  metadata->image_targets = Uptane::Targets(json);
  json = Utils::parseJSON(req_response_table[0]["image_timestamp"]);
  metadata->image_timestamp = Uptane::TimestampMeta(json);
  json = Utils::parseJSON(req_response_table[0]["image_snapshot"]);
  metadata->image_snapshot = Uptane::Snapshot(json);

  return true;
}

void SQLStorage::clearMetadata() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  if (sqlite3_exec(db.get(), "DELETE FROM meta;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear meta: " << sqlite3_errmsg(db.get());
    return;
  }
}

void SQLStorage::storeDeviceId(const std::string& device_id) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  std::string req = "UPDATE OR REPLACE device_info SET device_id = '";
  req += device_id + "';";
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set device ID: " << sqlite3_errmsg(db.get());
    return;
  }
}

bool SQLStorage::loadDeviceId(std::string* device_id) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT device_id FROM device_info LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  if (device_id) *device_id = req_response["result"];

  return true;
}

void SQLStorage::clearDeviceId() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  if (sqlite3_exec(db.get(), "UPDATE OR REPLACE device_info SET device_id = NULL;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear device ID: " << sqlite3_errmsg(db.get());
    return;
  }
}

void SQLStorage::storeEcuRegistered() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  std::string req = "UPDATE OR REPLACE device_info SET is_registered = 1";
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << sqlite3_errmsg(db.get());
    return;
  }
}

bool SQLStorage::loadEcuRegistered() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT is_registered FROM device_info LIMIT 1;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get device ID: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  return boost::lexical_cast<int>(req_response["result"]);
}

void SQLStorage::clearEcuRegistered() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  std::string req = "UPDATE OR REPLACE device_info SET is_registered = 0";
  if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << sqlite3_errmsg(db.get());
    return;
  }
}

void SQLStorage::storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) {
  if (serials.size() >= 1) {
    clearEcuSerials();
    SQLite3Guard db(config_.sqldb_path.c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
      return;
    }

    std::string req = "INSERT INTO ecu_serials VALUES  ('";
    req += serials[0].first + "', '";
    req += serials[0].second + "', 1);";
    if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
      LOG_ERROR << "Can't set ecu_serial: " << sqlite3_errmsg(db.get());
      return;
    }

    std::vector<std::pair<std::string, std::string> >::const_iterator it;
    for (it = serials.begin() + 1; it != serials.end(); it++) {
      std::string req = "INSERT INTO ecu_serials VALUES  ('";
      req += it->first + "', '";
      req += it->second + "', 0);";

      if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
        LOG_ERROR << "Can't set ecu_serial: " << sqlite3_errmsg(db.get());
        return;
      }
    }
  }
}

bool SQLStorage::loadEcuSerials(std::vector<std::pair<std::string, std::string> >* serials) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (sqlite3_exec(db.get(), "SELECT * FROM ecu_serials ORDER BY is_primary DESC;", callback, this, NULL) !=
      SQLITE_OK) {
    LOG_ERROR << "Can't get ecu_serials: " << sqlite3_errmsg(db.get());
    return false;
  }
  if (req_response_table.empty()) return false;

  std::vector<std::map<std::string, std::string> >::iterator it;
  for (it = req_response_table.begin(); it != req_response_table.end(); ++it) {
    serials->push_back(std::make_pair((*it)["serial"], (*it)["hardware_id"]));
  }

  return true;
}

void SQLStorage::clearEcuSerials() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  if (sqlite3_exec(db.get(), "DELETE FROM ecu_serials;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear ecu_serials: " << sqlite3_errmsg(db.get());
    return;
  }
}

void SQLStorage::storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) {
  if (ecus.size() >= 1) {
    clearMisconfiguredEcus();
    SQLite3Guard db(config_.sqldb_path.c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
      return;
    }

    std::vector<MisconfiguredEcu>::const_iterator it;
    for (it = ecus.begin(); it != ecus.end(); it++) {
      std::string req = "INSERT INTO misconfigured_ecus VALUES  ('";
      req += it->serial + "', '";
      req += it->hardware_id + "', ";
      req += Utils::intToString(it->state) + ");";

      if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
        LOG_ERROR << "Can't set misconfigured_ecus: " << sqlite3_errmsg(db.get());
        return;
      }
    }
  }
}

bool SQLStorage::loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (sqlite3_exec(db.get(), "SELECT * FROM misconfigured_ecus;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get misconfigured_ecus: " << sqlite3_errmsg(db.get());
    return false;
  }
  if (req_response_table.empty()) return false;

  std::vector<std::map<std::string, std::string> >::iterator it;
  for (it = req_response_table.begin(); it != req_response_table.end(); ++it) {
    ecus->push_back(MisconfiguredEcu((*it)["serial"], (*it)["hardware_id"],
                                     static_cast<EcuState>(boost::lexical_cast<int>((*it)["state"]))));
  }

  return true;
}

void SQLStorage::clearMisconfiguredEcus() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  if (sqlite3_exec(db.get(), "DELETE FROM misconfigured_ecus;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear misconfigured_ecus: " << sqlite3_errmsg(db.get());
    return;
  }
}

void SQLStorage::storeInstalledVersions(const std::map<std::string, std::string>& installed_versions) {
  if (installed_versions.size() >= 1) {
    SQLite3Guard db(config_.sqldb_path.c_str());

    clearInstalledVersions();

    std::map<std::string, std::string>::const_iterator it;
    for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
      std::string req = "INSERT INTO installed_versions VALUES  ('";
      req += it->first + "', '";
      req += it->second + "');";

      if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
        LOG_ERROR << "Can't set installed_versions: " << sqlite3_errmsg(db.get());
        return;
      }
    }
  }
}

bool SQLStorage::loadInstalledVersions(std::map<std::string, std::string>* installed_versions) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (sqlite3_exec(db.get(), "SELECT * FROM installed_versions;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get installed_versions: " << sqlite3_errmsg(db.get());
    return false;
  }
  if (req_response_table.empty()) return false;

  std::vector<std::map<std::string, std::string> >::iterator it;
  for (it = req_response_table.begin(); it != req_response_table.end(); ++it) {
    (*installed_versions)[(*it)["hash"]] = (*it)["name"];
  }

  return true;
}

void SQLStorage::clearInstalledVersions() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  if (sqlite3_exec(db.get(), "DELETE FROM installed_versions;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear installed_versions: " << sqlite3_errmsg(db.get());
    return;
  }
}

class SQLTargetWHandle : public StorageTargetWHandle {
 public:
  SQLTargetWHandle(const SQLStorage& storage, const std::string& filename, size_t size)
      : db_(storage.config_.sqldb_path.c_str()),
        storage_(storage),
        filename_(filename),
        expected_size_(size),
        written_size_(0),
        closed_(false),
        blob_(nullptr) {
    StorageTargetWHandle::WriteError exc("could not save file " + filename_ + " to sql storage");

    if (db_.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db_.get());
      throw exc;
    }

    // allocate a zero blob
    if (sqlite3_exec(db_.get(), "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't begin transaction: " << sqlite3_errmsg(db_.get());
      throw exc;
    }

    SQLGuardedStatement statement =
        sqlite3_prepare_guarded(db_.get(), "INSERT INTO target_images (filename, image_data) VALUES (?, ?)", -1);

    if (sqlite3_bind_text(statement.get(), 1, filename_.c_str(), -1, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_.get());
      throw exc;
    }
    if (sqlite3_bind_zeroblob(statement.get(), 2, expected_size_) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_.get());
      throw exc;
    }
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
      LOG_ERROR << "Statement step failure: " << sqlite3_errmsg(db_.get());
      throw exc;
    }

    // open the created blob for writing
    sqlite3_int64 row_id = sqlite3_last_insert_rowid(db_.get());

    if (sqlite3_blob_open(db_.get(), "main", "target_images", "image_data", row_id, 1, &blob_) != SQLITE_OK) {
      LOG_ERROR << "Could not open blob" << sqlite3_errmsg(db_.get());
      throw exc;
    }
  }

  ~SQLTargetWHandle() {
    if (!closed_) {
      LOG_WARNING << "Handle for file " << filename_ << " has not been committed or aborted, forcing abort";
      this->wabort();
    }
  }

  size_t wfeed(const uint8_t* buf, size_t size) override {
    if (sqlite3_blob_write(blob_, buf, static_cast<int>(size), written_size_) != SQLITE_OK) {
      LOG_ERROR << "Could not write in blob: " << sqlite3_errmsg(db_.get());
      return 0;
    }
    written_size_ += size;
    return size;
  }

  void wcommit() override {
    closed_ = true;
    sqlite3_blob_close(blob_);
    blob_ = nullptr;
    if (sqlite3_exec(db_.get(), "COMMIT TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't commit transaction: " << sqlite3_errmsg(db_.get());
      throw StorageTargetWHandle::WriteError("could not save file " + filename_ + " to sql storage");
    }
  }

  void wabort() noexcept override {
    closed_ = true;
    if (blob_ != nullptr) {
      sqlite3_blob_close(blob_);
      blob_ = nullptr;
    }
    if (sqlite3_exec(db_.get(), "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't rollback transaction: " << sqlite3_errmsg(db_.get());
    }
  }

 private:
  SQLite3Guard db_;
  const SQLStorage& storage_;
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
        storage_(storage),
        filename_(filename),
        size_(0),
        read_size_(0),
        closed_(false),
        blob_(nullptr) {
    StorageTargetRHandle::ReadError exc("could not read file " + filename_ + " from sql storage");

    if (db_.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db_.get());
      throw exc;
    }

    if (sqlite3_exec(db_.get(), "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't begin transaction: " << sqlite3_errmsg(db_.get());
      throw exc;
    }

    SQLGuardedStatement statement =
        sqlite3_prepare_guarded(db_.get(), "SELECT rowid FROM target_images WHERE filename = ?", -1);

    if (sqlite3_bind_text(statement.get(), 1, filename.c_str(), -1, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_.get());
      throw exc;
    }
    int err = sqlite3_step(statement.get());
    if (err == SQLITE_DONE) {
      LOG_ERROR << "No such file in db: " + filename_;
      throw exc;
    } else if (err != SQLITE_ROW) {
      LOG_ERROR << "Statement step failure: " << sqlite3_errmsg(db_.get());
      throw exc;
    }

    sqlite3_int64 row_id = sqlite3_column_int64(statement.get(), 0);

    if (sqlite3_blob_open(db_.get(), "main", "target_images", "image_data", row_id, 0, &blob_) != SQLITE_OK) {
      LOG_ERROR << "Could not open blob: " << sqlite3_errmsg(db_.get());
      throw exc;
    }
    size_ = sqlite3_blob_bytes(blob_);

    if (sqlite3_exec(db_.get(), "COMMIT TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK) {
      LOG_ERROR << "Can't commit transaction: " << sqlite3_errmsg(db_.get());
      throw exc;
    }
  }

  ~SQLTargetRHandle() {
    if (!closed_) {
      this->rclose();
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
      LOG_ERROR << "Could not read from blob: " << sqlite3_errmsg(db_.get());
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
  const SQLStorage& storage_;
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
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return;
  }

  SQLGuardedStatement statement = sqlite3_prepare_guarded(db.get(), "DELETE FROM target_images WHERE filename=?", -1);
  if (sqlite3_bind_text(statement.get(), 1, filename.c_str(), -1, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db.get());
    throw std::runtime_error("Could not remove target file");
  }
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    LOG_ERROR << "Statement step failure: " << sqlite3_errmsg(db.get());
    throw std::runtime_error("Could not remove target file");
  }

  if (sqlite3_changes(db.get()) != 1) {
    throw std::runtime_error("Target file " + filename + " not found");
  }
}

void SQLStorage::cleanUp() { boost::filesystem::remove_all(config_.sqldb_path); }

bool SQLStorage::tableSchemasEqual(const std::string& left, const std::string& right) {
  boost::char_separator<char> sep(" \"\t\r\n", "(),;");
  sql_tokenizer tokl(left, sep);
  sql_tokenizer tokr(right, sep);

  sql_tokenizer::iterator it_l;
  sql_tokenizer::iterator it_r;
  for (it_l = tokl.begin(), it_r = tokr.begin(); it_l != tokl.end() && it_r != tokr.end(); ++it_l, ++it_r) {
    if (*it_l != *it_r) return false;
  }
  return (it_l == tokl.end()) && (it_r == tokr.end());
}

boost::movelib::unique_ptr<std::map<std::string, std::string> > SQLStorage::parseSchema(int version) {
  boost::filesystem::path schema_file =
      config_.schemas_path / (std::string("schema.") + boost::lexical_cast<std::string>(version) + ".sql");
  std::string schema = Utils::readFile(schema_file.string());
  boost::movelib::unique_ptr<std::map<std::string, std::string> > result =
      boost::movelib::make_unique<std::map<std::string, std::string> >();
  std::vector<std::string> tokens;

  enum { STATE_INIT, STATE_CREATE, STATE_TABLE, STATE_NAME };
  boost::char_separator<char> sep(" \"\t\r\n", "(),;");
  sql_tokenizer tok(schema, sep);
  int parsing_state = STATE_INIT;

  std::string key;
  std::string value;
  for (sql_tokenizer::iterator it = tok.begin(); it != tok.end(); ++it) {
    std::string token = *it;
    if (value.empty())
      value = token;
    else
      value = value + " " + token;
    switch (parsing_state) {
      case STATE_INIT:
        if (token != "CREATE") {
          return boost::movelib::make_unique<std::map<std::string, std::string> >();
        }
        parsing_state = STATE_CREATE;
        break;
      case STATE_CREATE:
        if (token != "TABLE") {
          return boost::movelib::make_unique<std::map<std::string, std::string> >();
        }
        parsing_state = STATE_TABLE;
        break;
      case STATE_TABLE:
        if (token == "(" || token == ")" || token == "," || token == ";") {
          return boost::movelib::make_unique<std::map<std::string, std::string> >();
        }
        key = token;
        parsing_state = STATE_NAME;
        break;
      case STATE_NAME:
        if (token == ";") {
          (*result)[key] = value;
          key.clear();
          value.clear();
          parsing_state = STATE_INIT;
        }
        break;
    }
  }
  return boost::move(result);
}

std::string SQLStorage::getTableSchemaFromDb(const std::string& tablename) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return "";
  }

  request = kSqlGetSimple;
  req_response.clear();
  std::string req =
      std::string("SELECT sql FROM sqlite_master WHERE type='table' AND tbl_name='") + tablename + "' LIMIT 1;";
  if (sqlite3_exec(db.get(), req.c_str(), callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get schema of " << tablename << ": " << sqlite3_errmsg(db.get());
    return "";
  }

  return req_response["result"] + ";";
}

bool SQLStorage::dbMigrate() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return false;
  }

  int schema_version = getVersion();
  if (schema_version == kSqlSchemaVersion) {
    return true;
  }

  for (; schema_version <= kSqlSchemaVersion; schema_version++) {
    boost::filesystem::path migrate_script_path =
        config_.schemas_path / (std::string("migrate.") + boost::lexical_cast<std::string>(schema_version) + ".sql");
    std::string req = Utils::readFile(migrate_script_path.string());

    if (sqlite3_exec(db.get(), req.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
      LOG_ERROR << "Can't migrate db from version" << (schema_version - 1) << " to version " << schema_version << ": "
                << sqlite3_errmsg(db.get());
      return false;
    }
  }

  return true;
}

bool SQLStorage::dbCheck() {
  boost::movelib::unique_ptr<std::map<std::string, std::string> > tables = parseSchema(kSqlSchemaVersion);

  for (std::map<std::string, std::string>::iterator it = tables->begin(); it != tables->end(); ++it) {
    std::string schema_from_db = getTableSchemaFromDb(it->first);
    if (!tableSchemasEqual(schema_from_db, it->second)) {
      LOG_ERROR << "Schemas don't match for " << it->first;
      LOG_ERROR << "Expected " << it->second;
      LOG_ERROR << "Found " << schema_from_db;
      return false;
    }
  }
  return true;
}

bool SQLStorage::dbInit() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (sqlite3_exec(db.get(), "BEGIN TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't begin transaction: " << sqlite3_errmsg(db.get());
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (sqlite3_exec(db.get(), "SELECT count(*) FROM device_info;", callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get number of rows in device_info: " << sqlite3_errmsg(db.get());
    return false;
  }

  if (boost::lexical_cast<int>(req_response["result"]) < 1) {
    if (sqlite3_exec(db.get(), "INSERT INTO device_info DEFAULT VALUES;", NULL, NULL, NULL) != SQLITE_OK) {
      LOG_ERROR << "Can't set default values to device_info: " << sqlite3_errmsg(db.get());
      return false;
    }
  }

  if (sqlite3_exec(db.get(), "COMMIT TRANSACTION;", NULL, NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't commit transaction: " << sqlite3_errmsg(db.get());
    return false;
  }
  return true;
}

int SQLStorage::getVersion() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db.get());
    return -1;
  }

  request = kSqlGetSimple;
  req_response.clear();
  std::string req = std::string("SELECT version FROM version LIMIT 1;");
  if (sqlite3_exec(db.get(), req.c_str(), callback, this, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't get database version: " << sqlite3_errmsg(db.get());
    return -1;
  }

  try {
    return boost::lexical_cast<int>(req_response["result"]);
  } catch (const boost::bad_lexical_cast&) {
    return -1;
  }
}

int SQLStorage::callback(void* instance_, int numcolumns, char** values, char** columns) {
  SQLStorage* instance = static_cast<SQLStorage*>(instance_);

  switch (instance->request) {
    case kSqlGetSimple: {
      (void)numcolumns;
      (void)columns;
      if (values[0]) instance->req_response["result"] = values[0];
      break;
    }
    case kSqlGetTable: {
      std::map<std::string, std::string> row;
      for (int i = 0; i < numcolumns; ++i) {
        if (values[i] != NULL) {
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
