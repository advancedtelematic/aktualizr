#include "sqlstorage.h"

#include <iostream>
#include <memory>

#include "logging.h"
#include "sql_utils.h"
#include "utils.h"

SQLStorage::SQLStorage(const StorageConfig& config) : INvStorage(config) {
  if (!boost::filesystem::is_directory(config.schemas_path)) {
    throw std::runtime_error("Aktualizr installation incorrect. Schemas directory " + config.schemas_path.string() +
                             " missing");
  }

  boost::filesystem::create_directories(config.sqldb_path.parent_path());

  if (!dbMigrate()) {
    LOG_ERROR << "SQLite database migration failed";
    // Continue to run anyway, it can't be worse
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
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM primary_keys;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of public key table: " << db.errmsg();
    return;
  }
  const char* req;
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE primary_keys SET public = ?;";
  } else {
    req = "INSERT INTO primary_keys(public) VALUES (?);";
  }
  auto statement = db.prepareStatement<std::string>(req, public_key);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set public key: " << db.errmsg();
    return;
  }

  sync();
}

void SQLStorage::storePrimaryPrivate(const std::string& private_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM primary_keys;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get count of private key table: " << db.errmsg();
    return;
  }
  const char* req;
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE primary_keys SET private = ?;";
  } else {
    req = "INSERT INTO primary_keys(private) VALUES (?);";
  }
  auto statement = db.prepareStatement<std::string>(req, private_key);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set private key: " << db.errmsg();
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
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT public FROM primary_keys LIMIT 1;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get public key: " << db.errmsg();
    return false;
  }

  if (req_response.find("result") == req_response.end()) return false;

  if (public_key) *public_key = req_response["result"];

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

  if (req_response.find("result") == req_response.end()) return false;

  if (private_key) *private_key = req_response["result"];

  return true;
}

void SQLStorage::clearPrimaryKeys() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM primary_keys;", NULL, NULL) != SQLITE_OK) {
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
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE tls_creds SET ca_cert = ?;";
  } else {
    req = "INSERT INTO tls_creds(ca_cert) VALUES (?);";
  }
  auto statement = db.prepareStatement<SQLBlob>(req, ca);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set ca_cert: " << db.errmsg();
    return;
  }

  sync();
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
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE tls_creds SET client_cert = ?;";
  } else {
    req = "INSERT INTO tls_creds(client_cert) VALUES (?);";
  }
  auto statement = db.prepareStatement<SQLBlob>(req, cert);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set client_cert: " << db.errmsg();
    return;
  }

  sync();
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
  if (boost::lexical_cast<int>(req_response["result"])) {
    req = "UPDATE OR REPLACE tls_creds SET client_pkey = ?;";
  } else {
    req = "INSERT INTO tls_creds(client_pkey) VALUES (?);";
  }
  auto statement = db.prepareStatement<SQLBlob>(req, pkey);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set client_pkey: " << db.errmsg();
    return;
  }

  sync();
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
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM tls_creds;", NULL, NULL) != SQLITE_OK) {
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

  if (req_response.find("result") == req_response.end()) return false;

  if (ca) *ca = req_response["result"];

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

  if (req_response.find("result") == req_response.end()) return false;

  if (cert) *cert = req_response["result"];

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

  if (req_response.find("result") == req_response.end()) return false;

  if (pkey) *pkey = req_response["result"];

  return true;
}

void SQLStorage::storeMetadata(const Uptane::MetaPack& metadata) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  clearMetadata();

  std::vector<std::string> jsons;
  jsons.push_back(Json::FastWriter().write(metadata.director_root.toJson()));
  jsons.push_back(Json::FastWriter().write(metadata.director_targets.toJson()));
  jsons.push_back(Json::FastWriter().write(metadata.image_root.toJson()));
  jsons.push_back(Json::FastWriter().write(metadata.image_targets.toJson()));
  jsons.push_back(Json::FastWriter().write(metadata.image_timestamp.toJson()));
  jsons.push_back(Json::FastWriter().write(metadata.image_snapshot.toJson()));

  auto statement = db.prepareStatement<SQLBlob, SQLBlob, SQLBlob, SQLBlob, SQLBlob, SQLBlob>(
      "INSERT INTO meta VALUES (?,?,?,?,?,?);", jsons[0], jsons[1], jsons[2], jsons[3], jsons[4], jsons[5]);

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set metadata: " << db.errmsg();
    return;
  }

  sync();
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
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM meta;", NULL, NULL) != SQLITE_OK) {
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

  if (req_response.find("result") == req_response.end()) return false;

  if (device_id) *device_id = req_response["result"];

  return true;
}

void SQLStorage::clearDeviceId() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("UPDATE OR REPLACE device_info SET device_id = NULL;", NULL, NULL) != SQLITE_OK) {
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
  if (db.exec(req.c_str(), NULL, NULL) != SQLITE_OK) {
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

  if (req_response.find("result") == req_response.end()) return false;

  return boost::lexical_cast<int>(req_response["result"]);
}

void SQLStorage::clearEcuRegistered() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  std::string req = "UPDATE OR REPLACE device_info SET is_registered = 0";
  if (db.exec(req.c_str(), NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeEcuSerials(const std::vector<std::pair<std::string, std::string> >& serials) {
  if (serials.size() >= 1) {
    clearEcuSerials();
    SQLite3Guard db(config_.sqldb_path.c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db.errmsg();
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
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM ecu_serials;", NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear ecu_serials: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) {
  if (ecus.size() >= 1) {
    clearMisconfiguredEcus();
    SQLite3Guard db(config_.sqldb_path.c_str());

    if (db.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db.errmsg();
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
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM misconfigured_ecus;", NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear misconfigured_ecus: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeInstalledVersions(const std::map<std::string, InstalledVersion>& installed_versions) {
  if (installed_versions.size() >= 1) {
    SQLite3Guard db(config_.sqldb_path.c_str());

    clearInstalledVersions();
    std::map<std::string, InstalledVersion>::const_iterator it;
    for (it = installed_versions.begin(); it != installed_versions.end(); it++) {
      auto statement = db.prepareStatement<std::string, std::string, int>(
          "INSERT INTO installed_versions VALUES (?,?,?);", (*it).first, (*it).second.first,
          static_cast<int>((*it).second.second));

      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set installed_versions: " << db.errmsg();
        return;
      }
    }
  }
}

bool SQLStorage::loadInstalledVersions(std::map<std::string, InstalledVersion>* installed_versions) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  request = kSqlGetTable;
  req_response_table.clear();
  if (db.exec("SELECT * FROM installed_versions;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get installed_versions: " << db.errmsg();
    return false;
  }
  if (req_response_table.empty()) return false;

  std::vector<std::map<std::string, std::string> >::iterator it;
  for (it = req_response_table.begin(); it != req_response_table.end(); ++it) {
    (*installed_versions)[(*it)["hash"]].first = (*it)["name"];
    (*installed_versions)[(*it)["hash"]].second = !!boost::lexical_cast<int>((*it)["is_current"]);
  }

  return true;
}

void SQLStorage::clearInstalledVersions() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM installed_versions;", NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't clear installed_versions: " << db.errmsg();
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
      LOG_ERROR << "Can't open database: " << db_.errmsg();
      throw exc;
    }

    // allocate a zero blob
    if (db_.exec("BEGIN TRANSACTION;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't begin transaction: " << db_.errmsg();
      throw exc;
    }

    auto statement = db_.prepareStatement<std::string, SQLZeroBlob>(
        "INSERT INTO target_images (filename, image_data) VALUES (?, ?);", filename_, SQLZeroBlob{expected_size_});

    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Statement step failure: " << db_.errmsg();
      throw exc;
    }

    // open the created blob for writing
    sqlite3_int64 row_id = sqlite3_last_insert_rowid(db_.get());

    if (sqlite3_blob_open(db_.get(), "main", "target_images", "image_data", row_id, 1, &blob_) != SQLITE_OK) {
      LOG_ERROR << "Could not open blob" << db_.errmsg();
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
    if (db_.exec("COMMIT TRANSACTION;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't commit transaction: " << db_.errmsg();
      throw StorageTargetWHandle::WriteError("could not save file " + filename_ + " to sql storage");
    }
  }

  void wabort() noexcept override {
    closed_ = true;
    if (blob_ != nullptr) {
      sqlite3_blob_close(blob_);
      blob_ = nullptr;
    }
    if (db_.exec("ROLLBACK TRANSACTION;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't rollback transaction: " << db_.errmsg();
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
      LOG_ERROR << "Can't open database: " << db_.errmsg();
      throw exc;
    }

    if (db_.exec("BEGIN TRANSACTION;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't begin transaction: " << db_.errmsg();
      throw exc;
    }

    auto statement = db_.prepareStatement<std::string>("SELECT rowid FROM target_images WHERE filename = ?;", filename);

    int err = statement.step();
    if (err == SQLITE_DONE) {
      LOG_ERROR << "No such file in db: " + filename_;
      throw exc;
    } else if (err != SQLITE_ROW) {
      LOG_ERROR << "Statement step failure: " << db_.errmsg();
      throw exc;
    }

    sqlite3_int64 row_id = sqlite3_column_int64(statement.get(), 0);

    if (sqlite3_blob_open(db_.get(), "main", "target_images", "image_data", row_id, 0, &blob_) != SQLITE_OK) {
      LOG_ERROR << "Could not open blob: " << db_.errmsg();
      throw exc;
    }
    size_ = sqlite3_blob_bytes(blob_);

    if (db_.exec("COMMIT TRANSACTION;", NULL, NULL) != SQLITE_OK) {
      LOG_ERROR << "Can't commit transaction: " << db_.errmsg();
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
  }

  // hack here: getVersion() returns -1 if the database didn't exist, so 'migrate.0.sql' will be run in this case
  for (; schema_version < kSqlSchemaVersion; schema_version++) {
    boost::filesystem::path migrate_script_path =
        config_.schemas_path /
        (std::string("migrate.") + boost::lexical_cast<std::string>(schema_version + 1) + ".sql");
    std::string req = Utils::readFile(migrate_script_path.string());

    if (db.exec(req.c_str(), NULL, NULL) != SQLITE_OK) {
      LOG_ERROR << "Can't migrate db from version" << (schema_version - 1) << " to version " << schema_version << ": "
                << db.errmsg();
      return false;
    }
  }

  return true;
}

bool SQLStorage::dbInit() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.exec("BEGIN TRANSACTION;", NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't begin transaction: " << db.errmsg();
    return false;
  }

  request = kSqlGetSimple;
  req_response.clear();
  if (db.exec("SELECT count(*) FROM device_info;", callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get number of rows in device_info: " << db.errmsg();
    return false;
  }

  if (boost::lexical_cast<int>(req_response["result"]) < 1) {
    if (db.exec("INSERT INTO device_info DEFAULT VALUES;", NULL, NULL) != SQLITE_OK) {
      LOG_ERROR << "Can't set default values to device_info: " << db.errmsg();
      return false;
    }
  }

  if (db.exec("COMMIT TRANSACTION;", NULL, NULL) != SQLITE_OK) {
    LOG_ERROR << "Can't commit transaction: " << db.errmsg();
    return false;
  }
  return true;
}

int SQLStorage::getVersion() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return -1;
  }

  request = kSqlGetSimple;
  req_response.clear();
  std::string req = std::string("SELECT version FROM version LIMIT 1;");
  if (db.exec(req.c_str(), callback, this) != SQLITE_OK) {
    LOG_ERROR << "Can't get database version: " << db.errmsg();
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
