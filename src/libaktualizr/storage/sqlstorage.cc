#include "sqlstorage.h"

#include <sys/stat.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <libaktualizr/utils.h>

#include "logging/logging.h"
#include "sql_utils.h"

// find metadata with version set to -1 (e.g. after migration) and assign proper version to it
void SQLStorage::cleanMetaVersion(Uptane::RepositoryType repo, const Uptane::Role& role) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto statement = db.prepareStatement<int, int, int>(
      "SELECT meta FROM meta WHERE (repo=? AND meta_type=? AND version=?);", static_cast<int>(repo), role.ToInt(), -1);

  int result = statement.step();

  if (result == SQLITE_DONE) {
    LOG_TRACE << "meta with role " << role.ToString() << " in repo " << repo.toString() << " not present in db";
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

SQLStorage::SQLStorage(const StorageConfig& config, bool readonly)
    : SQLStorageBase(config.sqldb_path.get(config.path), readonly, libaktualizr_schema_migrations,
                     libaktualizr_schema_rollback_migrations, libaktualizr_current_schema,
                     libaktualizr_current_schema_version),
      INvStorage(config) {
  try {
    cleanMetaVersion(Uptane::RepositoryType::Director(), Uptane::Role::Root());
    cleanMetaVersion(Uptane::RepositoryType::Image(), Uptane::Role::Root());
  } catch (...) {
    LOG_ERROR << "SQLite database metadata version migration failed";
  }
}

void SQLStorage::storePrimaryKeys(const std::string& public_key, const std::string& private_key) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string>(
      "INSERT OR REPLACE INTO primary_keys(unique_mark,public,private) VALUES (0,?,?);", public_key, private_key);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set Primary keys: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) const {
  return loadPrimaryPublic(public_key) && loadPrimaryPrivate(private_key);
}

bool SQLStorage::loadPrimaryPublic(std::string* public_key) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT public FROM primary_keys LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "No public key in db";
    return false;
  } else if (result != SQLITE_ROW) {
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

bool SQLStorage::loadPrimaryPrivate(std::string* private_key) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT private FROM primary_keys LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "No private key in db";
    return false;
  } else if (result != SQLITE_ROW) {
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
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM primary_keys;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear Primary keys: " << db.errmsg();
    return;
  }
}

void SQLStorage::saveSecondaryInfo(const Uptane::EcuSerial& ecu_serial, const std::string& sec_type,
                                   const PublicKey& public_key) {
  SQLite3Guard db = dbConnection();

  std::stringstream key_type_ss;
  key_type_ss << public_key.Type();
  std::string key_type_str;
  key_type_str = key_type_ss.str();
  key_type_str.erase(std::remove(key_type_str.begin(), key_type_str.end(), '"'), key_type_str.end());

  db.beginTransaction();

  auto statement =
      db.prepareStatement<std::string>("SELECT count(*) FROM secondary_ecus WHERE serial = ?;", ecu_serial.ToString());
  if (statement.step() != SQLITE_ROW) {
    throw std::runtime_error(db.errmsg().insert(0, "Can't get count of secondary_ecus table: "));
  }

  const char* req;
  if (statement.get_result_col_int(0) != 0) {
    req = "UPDATE secondary_ecus SET sec_type = ?, public_key_type = ?, public_key = ? WHERE serial = ?;";
  } else {
    req =
        "INSERT INTO secondary_ecus (serial, sec_type, public_key_type, public_key) SELECT "
        "serial,?,?,? FROM ecus WHERE (serial = ? AND is_primary = 0);";
  }

  statement = db.prepareStatement<std::string, std::string, std::string, std::string>(
      req, sec_type, key_type_str, public_key.Value(), ecu_serial.ToString());
  if (statement.step() != SQLITE_DONE || sqlite3_changes(db.get()) != 1) {
    throw std::runtime_error(db.errmsg().insert(0, "Can't save Secondary key: "));
  }

  db.commitTransaction();
}

void SQLStorage::saveSecondaryData(const Uptane::EcuSerial& ecu_serial, const std::string& data) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto statement =
      db.prepareStatement<std::string>("SELECT count(*) FROM secondary_ecus WHERE serial = ?;", ecu_serial.ToString());
  if (statement.step() != SQLITE_ROW) {
    throw std::runtime_error(db.errmsg().insert(0, "Can't get count of secondary_ecus table: "));
  }

  const char* req;
  if (statement.get_result_col_int(0) != 0) {
    req = "UPDATE secondary_ecus SET extra = ? WHERE serial = ?;";
  } else {
    req = "INSERT INTO secondary_ecus (extra, serial) VALUES (?,?);";
  }

  statement = db.prepareStatement<std::string, std::string>(req, data, ecu_serial.ToString());
  if (statement.step() != SQLITE_DONE || sqlite3_changes(db.get()) != 1) {
    throw std::runtime_error(db.errmsg().insert(0, "Can't save Secondary data: "));
  }

  db.commitTransaction();
}

bool SQLStorage::loadSecondaryInfo(const Uptane::EcuSerial& ecu_serial, SecondaryInfo* secondary) const {
  SQLite3Guard db = dbConnection();

  SecondaryInfo new_sec{};

  auto statement = db.prepareStatement<std::string>(
      "SELECT serial, hardware_id, sec_type, public_key_type, public_key, extra FROM ecus LEFT JOIN secondary_ecus "
      "USING "
      "(serial) WHERE (serial = ? AND is_primary = 0);",
      ecu_serial.ToString());
  int statement_state = statement.step();
  if (statement_state == SQLITE_DONE) {
    LOG_TRACE << "Secondary ECU " << ecu_serial << " not found";
    return false;
  } else if (statement_state != SQLITE_ROW) {
    LOG_ERROR << "Cannot load Secondary info: " << db.errmsg();
    return false;
  }

  try {
    Uptane::EcuSerial serial = Uptane::EcuSerial(statement.get_result_col_str(0).value());
    Uptane::HardwareIdentifier hw_id = Uptane::HardwareIdentifier(statement.get_result_col_str(1).value());
    std::string sec_type = statement.get_result_col_str(2).value_or("");
    std::string kt_str = statement.get_result_col_str(3).value_or("");
    PublicKey key;
    if (kt_str != "") {
      KeyType key_type;
      std::stringstream(kt_str) >> key_type;
      key = PublicKey(statement.get_result_col_str(4).value_or(""), key_type);
    }
    std::string extra = statement.get_result_col_str(5).value_or("");
    new_sec = SecondaryInfo{serial, hw_id, sec_type, key, extra};
  } catch (const boost::bad_optional_access&) {
    return false;
  }

  if (secondary != nullptr) {
    *secondary = std::move(new_sec);
  }

  return true;
}

bool SQLStorage::loadSecondariesInfo(std::vector<SecondaryInfo>* secondaries) const {
  SQLite3Guard db = dbConnection();

  std::vector<SecondaryInfo> new_secs;

  bool empty = true;

  int statement_state;
  auto statement = db.prepareStatement(
      "SELECT serial, hardware_id, sec_type, public_key_type, public_key, extra FROM ecus LEFT JOIN secondary_ecus "
      "USING "
      "(serial) WHERE is_primary = 0 ORDER BY ecus.id;");
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    try {
      Uptane::EcuSerial serial = Uptane::EcuSerial(statement.get_result_col_str(0).value());
      Uptane::HardwareIdentifier hw_id = Uptane::HardwareIdentifier(statement.get_result_col_str(1).value());
      std::string sec_type = statement.get_result_col_str(2).value_or("");
      std::string kt_str = statement.get_result_col_str(3).value_or("");
      PublicKey key;
      if (kt_str != "") {
        KeyType key_type;
        std::stringstream(kt_str) >> key_type;
        key = PublicKey(statement.get_result_col_str(4).value_or(""), key_type);
      }
      std::string extra = statement.get_result_col_str(5).value_or("");
      new_secs.emplace_back(SecondaryInfo{serial, hw_id, sec_type, key, extra});
      empty = false;
    } catch (const boost::bad_optional_access&) {
      continue;
    }
  }
  if (statement_state != SQLITE_DONE) {
    LOG_ERROR << "Can't load Secondary info" << db.errmsg();
  }

  if (secondaries != nullptr) {
    *secondaries = std::move(new_secs);
  }

  return !empty;
}

void SQLStorage::storeTlsCreds(const std::string& ca, const std::string& cert, const std::string& pkey) {
  storeTlsCa(ca);
  storeTlsCert(cert);
  storeTlsPkey(pkey);
}

void SQLStorage::storeTlsCa(const std::string& ca) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

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

  db.commitTransaction();
}

void SQLStorage::storeTlsCert(const std::string& cert) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

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

  db.commitTransaction();
}

void SQLStorage::storeTlsPkey(const std::string& pkey) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

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

  db.commitTransaction();
}

bool SQLStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) const {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto statement = db.prepareStatement("SELECT ca_cert, client_cert, client_pkey FROM tls_creds LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "Tls creds not present";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get tls_creds: " << db.errmsg();
    return false;
  }

  std::string ca_v;
  std::string cert_v;
  std::string pkey_v;
  try {
    ca_v = statement.get_result_col_str(0).value();
    cert_v = statement.get_result_col_str(1).value();
    pkey_v = statement.get_result_col_str(2).value();
  } catch (const boost::bad_optional_access&) {
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

  db.commitTransaction();

  return true;
}

void SQLStorage::clearTlsCreds() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM tls_creds;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear tls_creds: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadTlsCa(std::string* ca) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT ca_cert FROM tls_creds LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "ca_cert not present";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get ca_cert: " << db.errmsg();
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

bool SQLStorage::loadTlsCert(std::string* cert) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT client_cert FROM tls_creds LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "client_cert not present in db";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get client_cert: " << db.errmsg();
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

bool SQLStorage::loadTlsPkey(std::string* pkey) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT client_pkey FROM tls_creds LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "client_pkey not present in db";
    return false;
  } else if (result != SQLITE_ROW) {
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
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto del_statement =
      db.prepareStatement<int, int, int>("DELETE FROM meta WHERE (repo=? AND meta_type=? AND version=?);",
                                         static_cast<int>(repo), Uptane::Role::Root().ToInt(), version.version());

  if (del_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't clear Root metadata: " << db.errmsg();
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

void SQLStorage::storeNonRoot(const std::string& data, Uptane::RepositoryType repo, const Uptane::Role role) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto del_statement = db.prepareStatement<int, int>("DELETE FROM meta WHERE (repo=? AND meta_type=?);",
                                                     static_cast<int>(repo), role.ToInt());

  if (del_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't clear " << role.ToString() << " metadata: " << db.errmsg();
    return;
  }

  auto ins_statement =
      db.prepareStatement<SQLBlob, int, int, int>("INSERT INTO meta VALUES (?, ?, ?, ?);", SQLBlob(data),
                                                  static_cast<int>(repo), role.ToInt(), Uptane::Version().version());

  if (ins_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't add " << role.ToString() << "metadata: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadRoot(std::string* data, Uptane::RepositoryType repo, Uptane::Version version) const {
  SQLite3Guard db = dbConnection();

  // version < 0 => latest metadata requested
  if (version.version() < 0) {
    auto statement = db.prepareStatement<int, int>(
        "SELECT meta FROM meta WHERE (repo=? AND meta_type=?) ORDER BY version DESC LIMIT 1;", static_cast<int>(repo),
        Uptane::Role::Root().ToInt());
    int result = statement.step();

    if (result == SQLITE_DONE) {
      LOG_TRACE << "Root metadata not present";
      return false;
    } else if (result != SQLITE_ROW) {
      LOG_ERROR << "Can't get Root metadata: " << db.errmsg();
      return false;
    }
    if (data != nullptr) {
      *data = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));
    }
  } else {
    auto statement =
        db.prepareStatement<int, int, int>("SELECT meta FROM meta WHERE (repo=? AND meta_type=? AND version=?);",
                                           static_cast<int>(repo), Uptane::Role::Root().ToInt(), version.version());

    int result = statement.step();

    if (result == SQLITE_DONE) {
      LOG_TRACE << "Root metadata not present";
      return false;
    } else if (result != SQLITE_ROW) {
      LOG_ERROR << "Can't get Root metadata: " << db.errmsg();
      return false;
    }

    const auto blob = reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0));
    if (blob == nullptr) {
      LOG_ERROR << "Can't get Root metadata: " << db.errmsg();
      return false;
    }

    if (data != nullptr) {
      *data = std::string(blob);
    }
  }

  return true;
}

bool SQLStorage::loadNonRoot(std::string* data, Uptane::RepositoryType repo, const Uptane::Role role) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<int, int>(
      "SELECT meta FROM meta WHERE (repo=? AND meta_type=?) ORDER BY version DESC LIMIT 1;", static_cast<int>(repo),
      role.ToInt());
  int result = statement.step();

  if (result == SQLITE_DONE) {
    LOG_TRACE << role.ToString() << " metadata not present";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get " << role.ToString() << " metadata: " << db.errmsg();
    return false;
  }
  if (data != nullptr) {
    *data = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));
  }

  return true;
}

void SQLStorage::clearNonRootMeta(Uptane::RepositoryType repo) {
  SQLite3Guard db = dbConnection();

  auto del_statement =
      db.prepareStatement<int>("DELETE FROM meta WHERE (repo=? AND meta_type != 0);", static_cast<int>(repo));

  if (del_statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't clear metadata: " << db.errmsg();
  }
}

void SQLStorage::clearMetadata() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM meta;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear metadata: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeDelegation(const std::string& data, const Uptane::Role role) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto statement = db.prepareStatement<SQLBlob, std::string>("INSERT OR REPLACE INTO delegations VALUES (?, ?);",
                                                             SQLBlob(data), role.ToString());

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't add delegation metadata: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadDelegation(std::string* data, const Uptane::Role role) const {
  SQLite3Guard db = dbConnection();

  auto statement =
      db.prepareStatement<std::string>("SELECT meta FROM delegations WHERE role_name=? LIMIT 1;", role.ToString());
  int result = statement.step();

  if (result == SQLITE_DONE) {
    LOG_TRACE << "Delegations metadata not present";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get delegations metadata: " << db.errmsg();
    return false;
  }
  if (data != nullptr) {
    *data = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));
  }

  return true;
}

bool SQLStorage::loadAllDelegations(std::vector<std::pair<Uptane::Role, std::string>>& data) const {
  bool result = false;

  try {
    SQLite3Guard db = dbConnection();

    auto statement = db.prepareStatement("SELECT meta, role_name FROM delegations;");
    auto statement_state = statement.step();

    if (statement_state == SQLITE_DONE) {
      LOG_TRACE << "Delegations metadata are not present";
      return true;
    } else if (statement_state != SQLITE_ROW) {
      LOG_ERROR << "Can't get delegations metadata: " << db.errmsg();
      return false;
    }

    do {
      data.emplace_back(Uptane::Role::Delegation(statement.get_result_col_str(1).value()),
                        statement.get_result_col_blob(0).value());
    } while ((statement_state = statement.step()) == SQLITE_ROW);

    result = true;
  } catch (const std::exception& exc) {
    LOG_ERROR << "Failed to fetch records from `delegations` table: " << exc.what();
  }

  return result;
}

void SQLStorage::deleteDelegation(const Uptane::Role role) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string>("DELETE FROM delegations WHERE role_name=?;", role.ToString());
  statement.step();
}

void SQLStorage::clearDelegations() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM delegations;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear delegations metadata: " << db.errmsg();
  }
}

void SQLStorage::storeDeviceId(const std::string& device_id) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string>(
      "INSERT OR REPLACE INTO device_info(unique_mark,device_id,is_registered) VALUES(0,?,0);", device_id);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set device ID: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadDeviceId(std::string* device_id) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT device_id FROM device_info LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "device_id not present in db";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get device ID: " << db.errmsg();
    return false;
  }

  auto did = statement.get_result_col_str(0);
  if (did == boost::none) {
    LOG_ERROR << "Empty device ID" << db.errmsg();
    return false;
  }

  if (device_id != nullptr) {
    *device_id = std::move(did.value());
  }

  return true;
}

void SQLStorage::clearDeviceId() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM device_info;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear device ID: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeEcuRegistered() {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto statement = db.prepareStatement("SELECT count(*) FROM device_info;");
  if (statement.step() != SQLITE_ROW) {
    throw std::runtime_error("Could not get device_info count");
  }
  if (statement.get_result_col_int(0) != 1) {
    throw std::runtime_error("Cannot set ECU registered if no device_info set");
  }

  std::string req = "UPDATE device_info SET is_registered = 1";
  if (db.exec(req.c_str(), nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadEcuRegistered() const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT is_registered FROM device_info LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get is_registered in device_info " << db.errmsg();
    return false;
  }

  return statement.get_result_col_int(0) != 0;
}

void SQLStorage::clearEcuRegistered() {
  SQLite3Guard db = dbConnection();

  // note: if the table is empty, nothing is done but that's fine
  std::string req = "UPDATE device_info SET is_registered = 0";
  if (db.exec(req.c_str(), nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeNeedReboot() {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<int>("INSERT OR REPLACE INTO need_reboot(unique_mark,flag) VALUES(0,?);", 1);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set need_reboot: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadNeedReboot(bool* need_reboot) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT flag FROM need_reboot LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    if (need_reboot != nullptr) {
      *need_reboot = false;
    }
    return true;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get need_reboot: " << db.errmsg();
    return false;
  }

  auto flag = static_cast<bool>(statement.get_result_col_int(0));
  if (need_reboot != nullptr) {
    *need_reboot = flag;
  }

  return true;
}

void SQLStorage::clearNeedReboot() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM need_reboot;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear need_reboot: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeEcuSerials(const EcuSerials& serials) {
  if (serials.size() >= 1) {
    SQLite3Guard db = dbConnection();

    db.beginTransaction();

    if (db.exec("DELETE FROM ecus;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't clear ecus: " << db.errmsg();
      return;
    }

    // first is the Primary
    std::string serial = serials[0].first.ToString();
    std::string hwid = serials[0].second.ToString();
    {
      auto statement = db.prepareStatement<std::string, std::string>(
          "INSERT INTO ecus(id, serial,hardware_id,is_primary) VALUES (0, ?,?,1);", serial, hwid);
      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
        return;
      }

      // update lazily stored installed version
      auto statement_ivupdate = db.prepareStatement<std::string>(
          "UPDATE installed_versions SET ecu_serial = ? WHERE ecu_serial = '';", serial);

      if (statement_ivupdate.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
        return;
      }
    }

    for (auto it = serials.cbegin() + 1; it != serials.cend(); it++) {
      auto statement = db.prepareStatement<int64_t, std::string, std::string>(
          "INSERT INTO ecus(id,serial,hardware_id) VALUES (?,?,?);", it - serials.cbegin(), it->first.ToString(),
          it->second.ToString());

      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
        return;
      }
    }

    db.commitTransaction();
  }
}

bool SQLStorage::loadEcuSerials(EcuSerials* serials) const {
  SQLite3Guard db = dbConnection();

  // order by auto-incremented Primary key so that the ECU order is kept constant
  auto statement = db.prepareStatement("SELECT serial, hardware_id FROM ecus ORDER BY id;");
  int statement_state;

  EcuSerials new_serials;
  bool empty = true;
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    try {
      new_serials.emplace_back(Uptane::EcuSerial(statement.get_result_col_str(0).value()),
                               Uptane::HardwareIdentifier(statement.get_result_col_str(1).value()));
      empty = false;
    } catch (const boost::bad_optional_access&) {
      return false;
    }
  }

  if (statement_state != SQLITE_DONE) {
    LOG_ERROR << "Can't get ECU serials: " << db.errmsg();
    return false;
  }

  if (serials != nullptr) {
    *serials = std::move(new_serials);
  }

  return !empty;
}

void SQLStorage::clearEcuSerials() {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  if (db.exec("DELETE FROM ecus;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear ECUs: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM secondary_ecus;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear Secondary ECUs: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

void SQLStorage::storeCachedEcuManifest(const Uptane::EcuSerial& ecu_serial, const std::string& manifest) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string, std::string>(
      "UPDATE secondary_ecus SET manifest = ? WHERE (serial = ?);", manifest, ecu_serial.ToString());
  if (statement.step() != SQLITE_DONE || sqlite3_changes(db.get()) != 1) {
    LOG_ERROR << "Can't save Secondary manifest: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadCachedEcuManifest(const Uptane::EcuSerial& ecu_serial, std::string* manifest) const {
  SQLite3Guard db = dbConnection();

  std::string stmanifest;

  bool empty = false;

  auto statement = db.prepareStatement<std::string>("SELECT manifest FROM secondary_ecus WHERE (serial = ?);",
                                                    ecu_serial.ToString());

  if (statement.step() != SQLITE_ROW) {
    LOG_WARNING << "Could not find manifest for ECU " << ecu_serial;
    return false;
  } else {
    stmanifest = statement.get_result_col_str(0).value_or("");

    empty = stmanifest == "";
  }

  if (manifest != nullptr) {
    *manifest = std::move(stmanifest);
  }

  return !empty;
}

void SQLStorage::saveMisconfiguredEcu(const MisconfiguredEcu& ecu) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string, std::string, int>(
      "INSERT OR REPLACE INTO misconfigured_ecus VALUES (?,?,?);", ecu.serial.ToString(), ecu.hardware_id.ToString(),
      static_cast<int>(ecu.state));
  if (statement.step() != SQLITE_DONE) {
    throw std::runtime_error(db.errmsg().insert(0, "Can't save misconfigured_ecus: "));
  }
}

bool SQLStorage::loadMisconfiguredEcus(std::vector<MisconfiguredEcu>* ecus) const {
  SQLite3Guard db = dbConnection();

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
    } catch (const boost::bad_optional_access&) {
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
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM misconfigured_ecus;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear misconfigured_ecus: " << db.errmsg();
    return;
  }
}

void SQLStorage::saveInstalledVersion(const std::string& ecu_serial, const Uptane::Target& target,
                                      InstalledVersionUpdateMode update_mode) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  // either adds a new entry or update the last one's status

  // empty serial: use Primary
  std::string ecu_serial_real = ecu_serial;
  if (ecu_serial_real.empty()) {
    auto statement = db.prepareStatement("SELECT serial FROM ecus WHERE is_primary = 1;");
    if (statement.step() == SQLITE_ROW) {
      ecu_serial_real = statement.get_result_col_str(0).value();
    } else {
      LOG_WARNING << "Could not find Primary ECU serial, set to lazy init mode";
    }
  }

  std::string hashes_encoded = Hash::encodeVector(target.hashes());

  // get the last time this version was installed on this ecu
  boost::optional<int64_t> old_id;
  bool old_was_installed = false;
  {
    auto statement = db.prepareStatement<std::string>(
        "SELECT id, sha256, name, was_installed FROM installed_versions WHERE ecu_serial = ? ORDER BY id DESC "
        "LIMIT 1;",
        ecu_serial_real);

    if (statement.step() == SQLITE_ROW) {
      int64_t rid = statement.get_result_col_int(0);
      std::string rsha256 = statement.get_result_col_str(1).value_or("");
      std::string rname = statement.get_result_col_str(2).value_or("");
      bool rwasi = statement.get_result_col_int(3) == 1;

      if (rsha256 == target.sha256Hash() && rname == target.filename()) {
        old_id = rid;
        old_was_installed = rwasi;
      }
    }
  }

  if (update_mode == InstalledVersionUpdateMode::kCurrent) {
    // unset 'current' and 'pending' on all versions for this ecu
    auto statement = db.prepareStatement<std::string>(
        "UPDATE installed_versions SET is_current = 0, is_pending = 0 WHERE ecu_serial = ?", ecu_serial_real);
    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Can't set installed_versions: " << db.errmsg();
      return;
    }
  } else if (update_mode == InstalledVersionUpdateMode::kPending) {
    // unset 'pending' on all versions for this ecu
    auto statement = db.prepareStatement<std::string>(
        "UPDATE installed_versions SET is_pending = 0 WHERE ecu_serial = ?", ecu_serial_real);
    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Can't set installed_versions: " << db.errmsg();
      return;
    }
  }

  if (!!old_id) {
    auto statement = db.prepareStatement<std::string, int, int, int64_t>(
        "UPDATE installed_versions SET correlation_id = ?, is_current = ?, is_pending = ?, was_installed = ? WHERE id "
        "= ?;",
        target.correlation_id(), static_cast<int>(update_mode == InstalledVersionUpdateMode::kCurrent),
        static_cast<int>(update_mode == InstalledVersionUpdateMode::kPending),
        static_cast<int>(update_mode == InstalledVersionUpdateMode::kCurrent || old_was_installed), old_id.value());

    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Can't set installed_versions: " << db.errmsg();
      return;
    }
  } else {
    std::string custom = Utils::jsonToCanonicalStr(target.custom_data());
    auto statement = db.prepareStatement<std::string, std::string, std::string, std::string, int64_t, std::string,
                                         std::string, int, int>(
        "INSERT INTO installed_versions(ecu_serial, sha256, name, hashes, length, custom_meta, correlation_id, "
        "is_current, is_pending, was_installed) VALUES (?,?,?,?,?,?,?,?,?,?);",
        ecu_serial_real, target.sha256Hash(), target.filename(), hashes_encoded, static_cast<int64_t>(target.length()),
        custom, target.correlation_id(), static_cast<int>(update_mode == InstalledVersionUpdateMode::kCurrent),
        static_cast<int>(update_mode == InstalledVersionUpdateMode::kPending),
        static_cast<int>(update_mode == InstalledVersionUpdateMode::kCurrent));

    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Can't set installed_versions: " << db.errmsg();
      return;
    }
  }

  db.commitTransaction();
}

static void loadEcuMap(SQLite3Guard& db, std::string& ecu_serial, Uptane::EcuMap& ecu_map) {
  // The Secondary only knows about itself and in its database it is considered
  // a Primary, for better or worse.
  if (ecu_serial.empty()) {
    auto statement = db.prepareStatement("SELECT serial FROM ecus WHERE is_primary = 1;");
    if (statement.step() == SQLITE_ROW) {
      ecu_serial = statement.get_result_col_str(0).value();
    } else if (statement.step() == SQLITE_DONE) {
      LOG_DEBUG << "No serial found in database for this ECU, defaulting to empty serial";
    } else {
      LOG_ERROR << "Error getting serial for this ECU, defaulting to empty serial: " << db.errmsg();
    }
  }

  if (!ecu_serial.empty()) {
    auto statement = db.prepareStatement<std::string>("SELECT hardware_id FROM ecus WHERE serial = ?;", ecu_serial);
    if (statement.step() == SQLITE_ROW) {
      ecu_map.insert(
          {Uptane::EcuSerial(ecu_serial), Uptane::HardwareIdentifier(statement.get_result_col_str(0).value())});
    } else if (statement.step() == SQLITE_DONE) {
      LOG_DEBUG << "No hardware ID found in database for ECU serial " << ecu_serial;
    } else {
      LOG_ERROR << "Error getting hardware ID for ECU serial " << ecu_serial << ": " << db.errmsg();
    }
  }
}

bool SQLStorage::loadInstallationLog(const std::string& ecu_serial, std::vector<Uptane::Target>* log,
                                     bool only_installed) const {
  SQLite3Guard db = dbConnection();

  std::string ecu_serial_real = ecu_serial;
  Uptane::EcuMap ecu_map;
  loadEcuMap(db, ecu_serial_real, ecu_map);

  std::string query =
      "SELECT id, sha256, name, hashes, length, correlation_id, custom_meta FROM installed_versions WHERE "
      "ecu_serial = ? ORDER BY id;";
  if (only_installed) {
    query =
        "SELECT id, sha256, name, hashes, length, correlation_id, custom_meta FROM installed_versions WHERE "
        "ecu_serial = ? AND was_installed = 1 ORDER BY id;";
  }

  auto statement = db.prepareStatement<std::string>(query, ecu_serial_real);
  int statement_state;

  std::vector<Uptane::Target> new_log;
  std::map<int64_t, size_t> ids_map;
  size_t k = 0;
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    try {
      auto id = statement.get_result_col_int(0);
      auto sha256 = statement.get_result_col_str(1).value();
      auto filename = statement.get_result_col_str(2).value();
      auto hashes_str = statement.get_result_col_str(3).value();
      auto length = statement.get_result_col_int(4);
      auto correlation_id = statement.get_result_col_str(5).value();
      auto custom_str = statement.get_result_col_str(6).value();

      // note: sha256 should always be present and is used to uniquely identify
      // a version. It should normally be part of the hash list as well.
      std::vector<Hash> hashes = Hash::decodeVector(hashes_str);

      auto find_sha256 =
          std::find_if(hashes.cbegin(), hashes.cend(), [](const Hash& h) { return h.type() == Hash::Type::kSha256; });
      if (find_sha256 == hashes.cend()) {
        LOG_WARNING << "No sha256 in hashes list";
        hashes.emplace_back(Hash::Type::kSha256, sha256);
      }

      Uptane::Target t(filename, ecu_map, hashes, static_cast<uint64_t>(length), correlation_id);
      if (!custom_str.empty()) {
        std::istringstream css(custom_str);
        std::string errs;
        Json::Value custom;
        if (Json::parseFromStream(Json::CharReaderBuilder(), css, &custom, nullptr)) {
          t.updateCustom(custom);
        } else {
          LOG_ERROR << "Unable to parse custom data: " << errs;
        }
      }
      new_log.emplace_back(t);

      ids_map[id] = k;
      k++;
    } catch (const boost::bad_optional_access&) {
      LOG_ERROR << "Incompleted installed version, keeping old one";
      return false;
    }
  }

  if (statement_state != SQLITE_DONE) {
    LOG_ERROR << "Can't get installed_versions: " << db.errmsg();
    return false;
  }

  if (log == nullptr) {
    return true;
  }

  *log = std::move(new_log);

  return true;
}

bool SQLStorage::loadInstalledVersions(const std::string& ecu_serial, boost::optional<Uptane::Target>* current_version,
                                       boost::optional<Uptane::Target>* pending_version) const {
  SQLite3Guard db = dbConnection();

  std::string ecu_serial_real = ecu_serial;
  Uptane::EcuMap ecu_map;
  loadEcuMap(db, ecu_serial_real, ecu_map);

  auto read_target = [&ecu_map](SQLiteStatement& statement) -> Uptane::Target {
    auto sha256 = statement.get_result_col_str(0).value();
    auto filename = statement.get_result_col_str(1).value();
    auto hashes_str = statement.get_result_col_str(2).value();
    auto length = statement.get_result_col_int(3);
    auto correlation_id = statement.get_result_col_str(4).value();
    auto custom_str = statement.get_result_col_str(5).value();

    // note: sha256 should always be present and is used to uniquely identify
    // a version. It should normally be part of the hash list as well.
    std::vector<Hash> hashes = Hash::decodeVector(hashes_str);

    auto find_sha256 =
        std::find_if(hashes.cbegin(), hashes.cend(), [](const Hash& h) { return h.type() == Hash::Type::kSha256; });
    if (find_sha256 == hashes.cend()) {
      LOG_WARNING << "No sha256 in hashes list";
      hashes.emplace_back(Hash::Type::kSha256, sha256);
    }
    Uptane::Target t(filename, ecu_map, hashes, static_cast<uint64_t>(length), correlation_id);
    if (!custom_str.empty()) {
      std::istringstream css(custom_str);
      Json::Value custom;
      std::string errs;
      if (Json::parseFromStream(Json::CharReaderBuilder(), css, &custom, &errs)) {
        t.updateCustom(custom);
      } else {
        LOG_ERROR << "Unable to parse custom data: " << errs;
      }
    }

    return t;
  };

  if (current_version != nullptr) {
    auto statement = db.prepareStatement<std::string>(
        "SELECT sha256, name, hashes, length, correlation_id, custom_meta FROM installed_versions WHERE "
        "ecu_serial = ? AND is_current = 1 LIMIT 1;",
        ecu_serial_real);

    if (statement.step() == SQLITE_ROW) {
      try {
        *current_version = read_target(statement);
      } catch (const boost::bad_optional_access&) {
        LOG_ERROR << "Could not read current installed version";
        return false;
      }
    } else {
      LOG_TRACE << "Cannot get current installed version: " << db.errmsg();
      *current_version = boost::none;
    }
  }

  if (pending_version != nullptr) {
    auto statement = db.prepareStatement<std::string>(
        "SELECT sha256, name, hashes, length, correlation_id, custom_meta FROM installed_versions WHERE "
        "ecu_serial = ? AND is_pending = 1 LIMIT 1;",
        ecu_serial_real);

    if (statement.step() == SQLITE_ROW) {
      try {
        *pending_version = read_target(statement);
      } catch (const boost::bad_optional_access&) {
        LOG_ERROR << "Could not read pending installed version";
        return false;
      }
    } else {
      LOG_TRACE << "Cannot get pending installed version: " << db.errmsg();
      *pending_version = boost::none;
    }
  }

  return true;
}

bool SQLStorage::hasPendingInstall() {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT count(*) FROM installed_versions where is_pending = 1");
  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Can't get tables count: " << db.errmsg();
    throw std::runtime_error("Could not count pending installations");
  }

  return statement.get_result_col_int(0) > 0;
}

void SQLStorage::getPendingEcus(std::vector<std::pair<Uptane::EcuSerial, Hash>>* pendingEcus) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement("SELECT ecu_serial, sha256 FROM installed_versions where is_pending = 1");
  int statement_result = statement.step();
  if (statement_result != SQLITE_DONE && statement_result != SQLITE_ROW) {
    throw std::runtime_error("Failed to get ECUs with a pending target installation: " + db.errmsg());
  }

  std::vector<std::pair<Uptane::EcuSerial, Hash>> ecu_res;

  if (statement_result == SQLITE_DONE) {
    // if there are no any record in the DB
    return;
  }

  for (; statement_result != SQLITE_DONE; statement_result = statement.step()) {
    std::string ecu_serial = statement.get_result_col_str(0).value();
    std::string hash = statement.get_result_col_str(1).value();
    ecu_res.emplace_back(std::make_pair(Uptane::EcuSerial(ecu_serial), Hash(Hash::Type::kSha256, hash)));
  }

  if (pendingEcus != nullptr) {
    *pendingEcus = std::move(ecu_res);
  }
}

void SQLStorage::clearInstalledVersions() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM installed_versions;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear installed_versions: " << db.errmsg();
    return;
  }
}

void SQLStorage::saveEcuInstallationResult(const Uptane::EcuSerial& ecu_serial,
                                           const data::InstallationResult& result) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string, int, std::string, std::string>(
      "INSERT OR REPLACE INTO ecu_installation_results (ecu_serial, success, result_code, description) VALUES "
      "(?,?,?,?);",
      ecu_serial.ToString(), static_cast<int>(result.success), result.result_code.toRepr(), result.description);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set ECU installation result: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadEcuInstallationResults(
    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>>* results) const {
  SQLite3Guard db = dbConnection();

  std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_res;

  // keep the same order as in ECUs (start with Primary)
  auto statement = db.prepareStatement(
      "SELECT ecu_serial, success, result_code, description FROM ecu_installation_results INNER JOIN ecus ON "
      "ecus.serial = ecu_serial ORDER BY ecus.id;");
  int statement_result = statement.step();
  if (statement_result != SQLITE_DONE && statement_result != SQLITE_ROW) {
    LOG_ERROR << "Can't get ecu_installation_results: " << db.errmsg();
    return false;
  }

  if (statement_result == SQLITE_DONE) {
    // if there are no any record in the DB
    return false;
  }

  for (; statement_result != SQLITE_DONE; statement_result = statement.step()) {
    try {
      std::string ecu_serial = statement.get_result_col_str(0).value();
      auto success = static_cast<bool>(statement.get_result_col_int(1));
      data::ResultCode result_code = data::ResultCode::fromRepr(statement.get_result_col_str(2).value());
      std::string description = statement.get_result_col_str(3).value();

      ecu_res.emplace_back(Uptane::EcuSerial(ecu_serial), data::InstallationResult(success, result_code, description));
    } catch (const boost::bad_optional_access&) {
      return false;
    }
  }

  if (results != nullptr) {
    *results = std::move(ecu_res);
  }

  return true;
}

void SQLStorage::storeDeviceInstallationResult(const data::InstallationResult& result, const std::string& raw_report,
                                               const std::string& correlation_id) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<int, std::string, std::string, std::string, std::string>(
      "INSERT OR REPLACE INTO device_installation_result (unique_mark, success, result_code, description, raw_report, "
      "correlation_id) "
      "VALUES (0,?,?,?,?,?);",
      static_cast<int>(result.success), result.result_code.toRepr(), result.description, raw_report, correlation_id);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set device installation result: " << db.errmsg();
    return;
  }
}

bool SQLStorage::storeDeviceInstallationRawReport(const std::string& raw_report) {
  SQLite3Guard db = dbConnection();
  auto statement = db.prepareStatement<std::string>("UPDATE device_installation_result SET raw_report=?;", raw_report);
  if (statement.step() != SQLITE_DONE || sqlite3_changes(db.get()) != 1) {
    LOG_ERROR << "Can't set device raw report result: " << db.errmsg();
    return false;
  }
  return true;
}

bool SQLStorage::loadDeviceInstallationResult(data::InstallationResult* result, std::string* raw_report,
                                              std::string* correlation_id) const {
  SQLite3Guard db = dbConnection();

  data::InstallationResult dev_res;
  std::string raw_report_res;
  std::string corrid_res;

  auto statement = db.prepareStatement(
      "SELECT success, result_code, description, raw_report, correlation_id FROM device_installation_result;");
  int statement_result = statement.step();
  if (statement_result == SQLITE_DONE) {
    LOG_TRACE << "No device installation result in db";
    return false;
  } else if (statement_result != SQLITE_ROW) {
    LOG_ERROR << "Can't get device_installation_result: " << db.errmsg();
    return false;
  }

  try {
    auto success = static_cast<bool>(statement.get_result_col_int(0));
    data::ResultCode result_code = data::ResultCode::fromRepr(statement.get_result_col_str(1).value());
    std::string description = statement.get_result_col_str(2).value();
    raw_report_res = statement.get_result_col_str(3).value();
    corrid_res = statement.get_result_col_str(4).value();

    dev_res = data::InstallationResult(success, result_code, description);
  } catch (const boost::bad_optional_access&) {
    return false;
  }

  if (result != nullptr) {
    *result = std::move(dev_res);
  }

  if (raw_report != nullptr) {
    *raw_report = std::move(raw_report_res);
  }

  if (correlation_id != nullptr) {
    *correlation_id = std::move(corrid_res);
  }

  return true;
}

void SQLStorage::saveEcuReportCounter(const Uptane::EcuSerial& ecu_serial, const int64_t counter) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string, int64_t>(
      "INSERT OR REPLACE INTO ecu_report_counter (ecu_serial, counter) VALUES "
      "(?,?);",
      ecu_serial.ToString(), counter);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set ECU counter: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadEcuReportCounter(std::vector<std::pair<Uptane::EcuSerial, int64_t>>* results) const {
  SQLite3Guard db = dbConnection();

  std::vector<std::pair<Uptane::EcuSerial, int64_t>> ecu_cnt;

  // keep the same order as in ECUs (start with Primary)
  auto statement = db.prepareStatement(
      "SELECT ecu_serial, counter FROM ecu_report_counter INNER JOIN ecus ON "
      "ecus.serial = ecu_serial ORDER BY ecus.id;");
  int statement_result = statement.step();
  if (statement_result != SQLITE_DONE && statement_result != SQLITE_ROW) {
    LOG_ERROR << "Can't get ecu_report_counter: " << db.errmsg();
    return false;
  }

  if (statement_result == SQLITE_DONE) {
    // if there are no any record in the DB
    return false;
  }

  for (; statement_result != SQLITE_DONE; statement_result = statement.step()) {
    try {
      std::string ecu_serial = statement.get_result_col_str(0).value();
      int64_t counter = statement.get_result_col_int(1);

      ecu_cnt.emplace_back(Uptane::EcuSerial(ecu_serial), counter);
    } catch (const boost::bad_optional_access&) {
      return false;
    }
  }

  if (results != nullptr) {
    *results = std::move(ecu_cnt);
  }

  return true;
}

void SQLStorage::saveReportEvent(const Json::Value& json_value) {
  std::string json_string = Utils::jsonToCanonicalStr(json_value);
  SQLite3Guard db = dbConnection();
  auto statement = db.prepareStatement<std::string>(
      "INSERT INTO report_events SELECT MAX(id) + 1, ? FROM report_events", json_string);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't save report event: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadReportEvents(Json::Value* report_array, int64_t* id_max) const {
  SQLite3Guard db = dbConnection();
  auto statement = db.prepareStatement("SELECT id, json_string FROM report_events;");
  int statement_result = statement.step();
  if (statement_result != SQLITE_DONE && statement_result != SQLITE_ROW) {
    LOG_ERROR << "Can't get report_events: " << db.errmsg();
    return false;
  }
  if (statement_result == SQLITE_DONE) {
    // if there are no any record in the DB
    return false;
  }
  *id_max = 0;
  for (; statement_result != SQLITE_DONE; statement_result = statement.step()) {
    try {
      int64_t id = statement.get_result_col_int(0);
      std::string json_string = statement.get_result_col_str(1).value();
      std::istringstream jss(json_string);
      Json::Value event_json;
      std::string errs;
      if (Json::parseFromStream(Json::CharReaderBuilder(), jss, &event_json, &errs)) {
        report_array->append(event_json);
        *id_max = (*id_max) > id ? (*id_max) : id;
      } else {
        LOG_ERROR << "Unable to parse event data: " << errs;
      }
    } catch (const boost::bad_optional_access&) {
      return false;
    }
  }

  return true;
}

void SQLStorage::deleteReportEvents(int64_t id_max) {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  auto statement = db.prepareStatement<int64_t>("DELETE FROM report_events WHERE id <= ?;", id_max);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't delete report_events";
  }

  db.commitTransaction();
}

void SQLStorage::clearInstallationResults() {
  SQLite3Guard db = dbConnection();

  db.beginTransaction();

  if (db.exec("DELETE FROM device_installation_result;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear device_installation_result: " << db.errmsg();
    return;
  }

  if (db.exec("DELETE FROM ecu_installation_results;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear ecu_installation_results: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

void SQLStorage::storeDeviceDataHash(const std::string& data_type, const std::string& hash) {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string, std::string>(
      "INSERT OR REPLACE INTO device_data(data_type,hash) VALUES (?,?);", data_type, hash);
  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set " << data_type << " hash: " << db.errmsg();
    throw std::runtime_error("Can't set " + data_type + " hash: " + db.errmsg());
  }
}

bool SQLStorage::loadDeviceDataHash(const std::string& data_type, std::string* hash) const {
  SQLite3Guard db = dbConnection();

  auto statement =
      db.prepareStatement<std::string>("SELECT hash FROM device_data WHERE data_type = ? LIMIT 1;", data_type);

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << data_type << " hash not present in db";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get " << data_type << " hash: " << db.errmsg();
    return false;
  }

  if (hash != nullptr) {
    *hash = statement.get_result_col_str(0).value();
  }

  return true;
}

void SQLStorage::clearDeviceData() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM device_data;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear device_data: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeTargetFilename(const std::string& targetname, const std::string& filename) const {
  SQLite3Guard db = dbConnection();
  auto statement = db.prepareStatement<std::string, std::string>(
      "INSERT OR REPLACE INTO target_images (targetname, filename) VALUES (?, ?);", targetname, filename);

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Statement step failure: " << db.errmsg();
    throw std::runtime_error("Could not write to db");
  }
}

std::string SQLStorage::getTargetFilename(const std::string& targetname) const {
  SQLite3Guard db = dbConnection();

  auto statement =
      db.prepareStatement<std::string>("SELECT filename FROM target_images WHERE targetname = ?;", targetname);

  switch (statement.step()) {
    case SQLITE_ROW:
      return statement.get_result_col_str(0).value();
    case SQLITE_DONE:
      return {};
    default:
      throw std::runtime_error(db.errmsg().insert(0, "Error reading target filename from db: "));
  }
}

std::vector<std::string> SQLStorage::getAllTargetNames() const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<>("SELECT targetname FROM target_images;");

  std::vector<std::string> names;

  int result = statement.step();
  while (result != SQLITE_DONE) {
    if (result != SQLITE_ROW) {
      LOG_ERROR << "Statement step failure: " << db.errmsg();
      throw std::runtime_error("Error getting target files");
    }
    names.push_back(statement.get_result_col_str(0).value());
    result = statement.step();
  }
  return names;
}

void SQLStorage::deleteTargetInfo(const std::string& targetname) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string>("DELETE FROM target_images WHERE targetname=?;", targetname);

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Statement step failure: " << db.errmsg();
    throw std::runtime_error("Could not remove target file");
  }
}

void SQLStorage::cleanUp() { boost::filesystem::remove_all(dbPath()); }
