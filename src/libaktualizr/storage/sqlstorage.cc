#include "sqlstorage.h"

#include <sys/stat.h>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "logging/logging.h"
#include "sql_utils.h"
#include "utilities/utils.h"

// find metadata with version set to -1 (e.g. after migration) and assign proper version to it
void SQLStorage::cleanMetaVersion(Uptane::RepositoryType repo, Uptane::Role role) {
  SQLite3Guard db(config_.sqldb_path.c_str());
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (!db.beginTransaction()) {
    return;
  }

  auto statement = db.prepareStatement<int, int, int>(
      "SELECT meta FROM meta WHERE (repo=? AND meta_type=? AND version=?);", static_cast<int>(repo), role.ToInt(), -1);

  int result = statement.step();

  if (result == SQLITE_DONE) {
    return;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get meta: " << db.errmsg();
    return;
  }
  std::string meta = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));

  int version = Uptane::extractVersionUntrusted(meta);
  if (version < 0) {
    LOG_ERROR << "Corrupted metadata";
    return;
  }

  // in there is already metadata with such version delete it
  statement = db.prepareStatement<int, int, int>("DELETE FROM meta WHERE (repo=? AND meta_type=? AND version=?);",
                                                 static_cast<int>(repo), role.ToInt(), version);

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't clear metadata: " << db.errmsg();
    return;
  }

  statement = db.prepareStatement<int, int, int, int>(
      "UPDATE meta SET version = ? WHERE (repo=? AND meta_type=? AND version=?);", version, static_cast<int>(repo),
      role.ToInt(), -1);

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't update metadata: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

SQLStorage::SQLStorage(const StorageConfig& config) : INvStorage(config) {
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

  try {
    cleanMetaVersion(Uptane::RepositoryType::Director, Uptane::Role::Root());
    cleanMetaVersion(Uptane::RepositoryType::Images, Uptane::Role::Root());
  } catch (...) {
    LOG_ERROR << "SQLite database metadata version migration failed";
  }
}

void SQLStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  auto statement = db.prepareStatement("SELECT count(*) FROM primary_keys;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get count of keys table: " << db.errmsg();
    return;
  }

  const char* req;
  if (statement.get_result_col_int(0) != 0) {
    req = "UPDATE OR REPLACE primary_keys SET (public, private) = (?,?);";
  } else {
    req = "INSERT INTO primary_keys(public,private) VALUES (?,?);";
  }

  statement = db.prepareStatement<std::string>(req, public_key, private_key);
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

  auto statement = db.prepareStatement("SELECT public FROM primary_keys LIMIT 1;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get public key: " << db.errmsg();
    return false;
  }

  auto pub = statement.get_result_col_str(0);
  if (pub == boost::none) {
    return false;
  }

  if (public_key != nullptr) {
    *public_key = std::move(pub.value());
  }

  return true;
}

bool SQLStorage::loadPrimaryPrivate(std::string* private_key) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  auto statement = db.prepareStatement("SELECT private FROM primary_keys LIMIT 1;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get private key: " << db.errmsg();
    return false;
  }

  auto priv = statement.get_result_col_str(0);
  if (priv == boost::none) {
    return false;
  }

  if (private_key != nullptr) {
    *private_key = std::move(priv.value());
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

  auto statement = db.prepareStatement("SELECT count(*) FROM tls_creds;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get count of tls_creds table: " << db.errmsg();
    return;
  }

  const char* req;
  if (statement.get_result_col_int(0) != 0) {
    req = "UPDATE OR REPLACE tls_creds SET ca_cert = ?;";
  } else {
    req = "INSERT INTO tls_creds(ca_cert) VALUES (?);";
  }

  statement = db.prepareStatement<SQLBlob>(req, SQLBlob(ca));
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

  auto statement = db.prepareStatement("SELECT count(*) FROM tls_creds;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get count of tls_creds table: " << db.errmsg();
    return;
  }

  const char* req;
  if (statement.get_result_col_int(0) != 0) {
    req = "UPDATE OR REPLACE tls_creds SET client_cert = ?;";
  } else {
    req = "INSERT INTO tls_creds(client_cert) VALUES (?);";
  }

  statement = db.prepareStatement<SQLBlob>(req, SQLBlob(cert));
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

  auto statement = db.prepareStatement("SELECT count(*) FROM tls_creds;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get count of tls_creds table: " << db.errmsg();
    return;
  }

  const char* req;
  if (statement.get_result_col_int(0) != 0) {
    req = "UPDATE OR REPLACE tls_creds SET client_pkey = ?;";
  } else {
    req = "INSERT INTO tls_creds(client_pkey) VALUES (?);";
  }

  statement = db.prepareStatement<SQLBlob>(req, SQLBlob(pkey));
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

  auto statement = db.prepareStatement("SELECT ca_cert, client_cert, client_pkey FROM tls_creds LIMIT 1;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get tls_creds: " << db.errmsg();
    return false;
  }

  std::string ca_v, cert_v, pkey_v;
  try {
    ca_v = statement.get_result_col_str(0).value();
    cert_v = statement.get_result_col_str(1).value();
    pkey_v = statement.get_result_col_str(2).value();
  } catch (boost::bad_optional_access) {
    return false;
  }

  if (ca != nullptr) {
    *ca = std::move(ca_v);
  }
  if (cert != nullptr) {
    *cert = std::move(cert_v);
  }
  if (pkey != nullptr) {
    *pkey = std::move(pkey_v);
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

  auto statement = db.prepareStatement("SELECT ca_cert FROM tls_creds LIMIT 1;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  auto ca_r = statement.get_result_col_str(0);
  if (ca_r == boost::none) {
    return false;
  }

  if (ca != nullptr) {
    *ca = std::move(ca_r.value());
  }

  return true;
}

bool SQLStorage::loadTlsCert(std::string* cert) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  auto statement = db.prepareStatement("SELECT client_cert FROM tls_creds LIMIT 1;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  auto cert_r = statement.get_result_col_str(0);
  if (cert_r == boost::none) {
    return false;
  }

  if (cert != nullptr) {
    *cert = std::move(cert_r.value());
  }

  return true;
}

bool SQLStorage::loadTlsPkey(std::string* pkey) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  auto statement = db.prepareStatement("SELECT client_pkey FROM tls_creds LIMIT 1;");

  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get client_pkey: " << db.errmsg();
    return false;
  }

  auto pkey_r = statement.get_result_col_str(0);
  if (pkey_r == boost::none) {
    return false;
  }

  if (pkey != nullptr) {
    *pkey = std::move(pkey_r.value());
  }

  return true;
}

void SQLStorage::storeRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Version version) {
  SQLite3Guard db(config_.sqldb_path.c_str());
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (!db.beginTransaction()) {
    return;
  }

  auto del_statement =
      db.prepareStatement<int, int, int>("DELETE FROM meta WHERE (repo=? AND meta_type=? AND version=?);",
                                         static_cast<int>(repo), Uptane::Role::Root().ToInt(), version.version());

  if (del_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't clear root metadata: " << db.errmsg();
    return;
  }

  auto ins_statement = db.prepareStatement<SQLBlob, int, int, int>("INSERT INTO meta VALUES (?, ?, ?, ?);",
                                                                   SQLBlob(data), static_cast<int>(repo),
                                                                   Uptane::Role::Root().ToInt(), version.version());

  if (ins_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't add metadata: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

void SQLStorage::storeNonRoot(const std::string& data, Uptane::RepositoryType repo, Uptane::Role role) {
  SQLite3Guard db(config_.sqldb_path.c_str());
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (!db.beginTransaction()) {
    return;
  }

  auto del_statement = db.prepareStatement<int, int>("DELETE FROM meta WHERE (repo=? AND meta_type=?);",
                                                     static_cast<int>(repo), role.ToInt());

  if (del_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't clear metadata: " << db.errmsg();
    return;
  }

  auto ins_statement =
      db.prepareStatement<SQLBlob, int, int, int>("INSERT INTO meta VALUES (?, ?, ?, ?);", SQLBlob(data),
                                                  static_cast<int>(repo), role.ToInt(), Uptane::Version().version());

  if (ins_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't add metadata: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) {
  SQLite3Guard db(config_.sqldb_path.c_str());
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  // version < 0 => latest metadata requested
  if (version.version() < 0) {
    auto statement = db.prepareStatement<int, int>(
        "SELECT meta FROM meta WHERE (repo=? AND meta_type=?) ORDER BY version DESC LIMIT 1;", static_cast<int>(repo),
        Uptane::Role::Root().ToInt());
    int result = statement.step();

    if (result == SQLITE_DONE) {
      LOG_TRACE << "Meta not present";
      return false;
    } else if (result != SQLITE_ROW) {
      LOG_ERROR << "Can't get meta: " << db.errmsg();
      return false;
    }
    *data = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));
  } else {
    auto statement =
        db.prepareStatement<int, int, int>("SELECT meta FROM meta WHERE (repo=? AND meta_type=? AND version=?);",
                                           static_cast<int>(repo), Uptane::Role::Root().ToInt(), version.version());

    int result = statement.step();

    if (result == SQLITE_DONE) {
      LOG_TRACE << "Meta not present";
      return false;
    } else if (result != SQLITE_ROW) {
      LOG_ERROR << "Can't get meta: " << db.errmsg();
      return false;
    }
    *data = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));
  }

  return true;
}

bool SQLStorage::loadNonRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Role role) {
  SQLite3Guard db(config_.sqldb_path.c_str());
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  auto statement = db.prepareStatement<int, int>(
      "SELECT meta FROM meta WHERE (repo=? AND meta_type=?) ORDER BY version DESC LIMIT 1;", static_cast<int>(repo),
      role.ToInt());
  int result = statement.step();

  if (result == SQLITE_DONE) {
    LOG_TRACE << "Meta not present";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get meta: " << db.errmsg();
    return false;
  }
  *data = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));

  return true;
}

void SQLStorage::clearNonRootMeta(Uptane::RepositoryType repo) {
  SQLite3Guard db(config_.sqldb_path.c_str());
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  auto del_statement =
      db.prepareStatement<int>("DELETE FROM meta WHERE (repo=? AND meta_type != 0);", static_cast<int>(repo));

  if (del_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't clear metadata: " << db.errmsg();
  }
}

void SQLStorage::clearMetadata() {
  SQLite3Guard db(config_.sqldb_path.c_str());
  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM meta;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear metadata: " << db.errmsg();
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

  auto statement = db.prepareStatement("SELECT device_id FROM device_info LIMIT 1;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  auto did = statement.get_result_col_str(0);
  if (did == boost::none) {
    return false;
  }

  if (device_id != nullptr) {
    *device_id = std::move(did.value());
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

  auto statement = db.prepareStatement("SELECT is_registered FROM device_info LIMIT 1;");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  return statement.get_result_col_int(0) != 0;
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

void SQLStorage::storeEcuSerials(const EcuSerials& serials) {
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

    std::string serial = serials[0].first.ToString();
    std::string hwid = serials[0].second.ToString();
    {
      auto statement =
          db.prepareStatement<std::string, std::string>("INSERT INTO ecu_serials VALUES (?,?,1);", serial, hwid);
      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
        return;
      }
    }

    EcuSerials::const_iterator it;
    for (it = serials.begin() + 1; it != serials.end(); it++) {
      auto statement = db.prepareStatement<std::string, std::string>("INSERT INTO ecu_serials VALUES (?,?,0);",
                                                                     it->first.ToString(), it->second.ToString());

      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
        return;
      }
    }

    db.commitTransaction();
  }
}

bool SQLStorage::loadEcuSerials(EcuSerials* serials) {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  auto statement = db.prepareStatement("SELECT serial, hardware_id FROM ecu_serials ORDER BY is_primary DESC;");
  int statement_state;

  EcuSerials new_serials;
  bool empty = true;
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    try {
      new_serials.emplace_back(Uptane::EcuSerial(statement.get_result_col_str(0).value()),
                               Uptane::HardwareIdentifier(statement.get_result_col_str(1).value()));
      empty = false;
    } catch (boost::bad_optional_access) {
      return false;
    }
  }

  if (statement_state != SQLITE_DONE) {
    LOG_ERROR << "Can't get ecu_serials: " << db.errmsg();
    return false;
  }

  if (serials != nullptr) {
    *serials = std::move(new_serials);
  }

  return !empty;
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
          "INSERT INTO misconfigured_ecus VALUES (?,?,?);", it->serial.ToString(), it->hardware_id.ToString(),
          static_cast<int>(it->state));

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

  auto statement = db.prepareStatement("SELECT serial, hardware_id, state FROM misconfigured_ecus;");
  int statement_state;

  std::vector<MisconfiguredEcu> new_ecus;
  bool empty = true;
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    try {
      new_ecus.emplace_back(Uptane::EcuSerial(statement.get_result_col_str(0).value()),
                            Uptane::HardwareIdentifier(statement.get_result_col_str(1).value()),
                            static_cast<EcuState>(statement.get_result_col_int(2)));
      empty = false;
    } catch (boost::bad_optional_access) {
      return false;
    }
  }

  if (statement_state != SQLITE_DONE) {
    LOG_ERROR << "Can't get misconfigured_ecus: " << db.errmsg();
    return false;
  }

  if (ecus != nullptr) {
    *ecus = std::move(new_ecus);
  }

  return !empty;
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

  auto statement = db.prepareStatement("SELECT name, hash, length, is_current FROM installed_versions;");
  int statement_state;

  std::vector<Uptane::Target> new_installed_versions;
  std::string new_hash;
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    try {
      Json::Value installed_version;
      auto name = statement.get_result_col_str(0).value();
      auto hash = statement.get_result_col_str(1).value();
      auto length = statement.get_result_col_int(2);
      auto is_current = statement.get_result_col_int(3) != 0;

      installed_version["hashes"]["sha256"] = hash;
      installed_version["length"] = Json::UInt64(length);
      if (is_current) {
        new_hash = hash;
      }
      std::string filename = name;
      new_installed_versions.emplace_back(filename, installed_version);
    } catch (boost::bad_optional_access) {
      LOG_ERROR << "Incompleted installed version, keeping old one";
      return current_hash;
    }
  }

  if (new_hash != "") {
    current_hash = new_hash;
  }

  if (statement_state != SQLITE_DONE) {
    LOG_ERROR << "Can't get installed_versions: " << db.errmsg();
    return current_hash;
  }

  if (installed_versions != nullptr) {
    *installed_versions = std::move(new_installed_versions);
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

    auto row_id = statement.get_result_col_int(0);

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

  auto schema = statement.get_result_col_str(0);
  if (schema == boost::none) {
    return "";
  }

  return schema.value() + ";";
}

bool SQLStorage::dbMigrate() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return false;
  }

  DbVersion schema_version = getVersion();

  if (schema_version == DbVersion::kInvalid) {
    LOG_ERROR << "Sqlite database file is invalid.";
    return false;
  }

  auto schema_num_version = static_cast<int32_t>(schema_version);
  if (schema_num_version == current_schema_version) {
    return true;
  }

  if (schema_num_version > current_schema_version) {
    LOG_ERROR << "Only forward migrations are supported. You cannot migrate to an older schema.";
    return false;
  }

  for (int k = schema_num_version + 1; k <= current_schema_version; k++) {
    if (db.exec(schema_migrations[k].c_str(), nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't migrate db from version" << (k - 1) << " to version " << k << ": " << db.errmsg();
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

  auto statement = db.prepareStatement("SELECT count(*) FROM device_info;");

  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get number of rows in device_info: " << db.errmsg();
    return false;
  }

  if (statement.get_result_col_int(0) < 1) {
    if (db.exec("INSERT INTO device_info DEFAULT VALUES;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't set default values to device_info: " << db.errmsg();
      return false;
    }
  }

  return db.commitTransaction();
}

DbVersion SQLStorage::getVersion() {
  SQLite3Guard db(config_.sqldb_path.c_str());

  if (db.get_rc() != SQLITE_OK) {
    LOG_ERROR << "Can't open database: " << db.errmsg();
    return DbVersion::kInvalid;
  }

  try {
    auto statement = db.prepareStatement("SELECT count(*) FROM sqlite_master WHERE type='table';");
    if (statement.step() != SQLITE_ROW) {
      LOG_ERROR << "Can't get tables count: " << db.errmsg();
      return DbVersion::kInvalid;
    }

    if (statement.get_result_col_int(0) == 0) {
      return DbVersion::kEmpty;
    }

    statement = db.prepareStatement("SELECT version FROM version LIMIT 1;");

    if (statement.step() != SQLITE_ROW) {
      LOG_ERROR << "Can't get database version: " << db.errmsg();
      return DbVersion::kInvalid;
    }

    try {
      return DbVersion(statement.get_result_col_int(0));
    } catch (const std::exception&) {
      return DbVersion::kInvalid;
    }
  } catch (SQLException) {
    return DbVersion::kInvalid;
  }
}
