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
void SQLStorage::cleanMetaVersion(Uptane::RepositoryType repo, const Uptane::Role& role) {
  SQLite3Guard db = dbConnection();

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
    return;
  }

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
    LOG_ERROR << "Can't set primary keys: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadPrimaryKeys(std::string* public_key, std::string* private_key) {
  return loadPrimaryPublic(public_key) && loadPrimaryPrivate(private_key);
}

bool SQLStorage::loadPrimaryPublic(std::string* public_key) {
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

bool SQLStorage::loadPrimaryPrivate(std::string* private_key) {
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
  SQLite3Guard db = dbConnection();

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
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

  db.commitTransaction();
}

void SQLStorage::storeTlsCert(const std::string& cert) {
  SQLite3Guard db = dbConnection();

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
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

  db.commitTransaction();
}

void SQLStorage::storeTlsPkey(const std::string& pkey) {
  SQLite3Guard db = dbConnection();

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
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

  db.commitTransaction();
}

bool SQLStorage::loadTlsCreds(std::string* ca, std::string* cert, std::string* pkey) {
  SQLite3Guard db = dbConnection();

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
    return false;
  }
  auto statement = db.prepareStatement("SELECT ca_cert, client_cert, client_pkey FROM tls_creds LIMIT 1;");

  int result = statement.step();
  if (result == SQLITE_DONE) {
    LOG_TRACE << "Tls creds not present";
    return false;
  } else if (result != SQLITE_ROW) {
    LOG_ERROR << "Can't get tls_creds: " << db.errmsg();
    return false;
  }

  std::string ca_v, cert_v, pkey_v;
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

bool SQLStorage::loadTlsCa(std::string* ca) {
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

bool SQLStorage::loadTlsCert(std::string* cert) {
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

bool SQLStorage::loadTlsPkey(std::string* pkey) {
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

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
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

void SQLStorage::storeNonRoot(const std::string& data, Uptane::RepositoryType repo, const Uptane::Role role) {
  SQLite3Guard db = dbConnection();

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
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
  SQLite3Guard db = dbConnection();

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
    if (data != nullptr) {
      *data = std::string(reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0)));
    }
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

    const auto blob = reinterpret_cast<const char*>(sqlite3_column_blob(statement.get(), 0));
    if (blob == nullptr) {
      LOG_ERROR << "Can't get meta: " << db.errmsg();
      return false;
    }

    if (data != nullptr) {
      *data = std::string(blob);
    }
  }

  return true;
}

bool SQLStorage::loadNonRoot(std::string* data, Uptane::RepositoryType repo, const Uptane::Role role) {
  SQLite3Guard db = dbConnection();

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

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
    return;
  }

  auto statement = db.prepareStatement<SQLBlob, std::string>("INSERT OR REPLACE INTO delegations VALUES (?, ?);",
                                                             SQLBlob(data), role.ToString());

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't add delegation metadata: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadDelegation(std::string* data, const Uptane::Role role) {
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

bool SQLStorage::loadDeviceId(std::string* device_id) {
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

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
    return;
  }

  auto statement = db.prepareStatement("SELECT count(*) FROM device_info;");
  if (statement.step() != SQLITE_ROW) {
    throw std::runtime_error("Could not get device_info count");
  }
  if (statement.get_result_col_int(0) != 1) {
    throw std::runtime_error("Cannot set ecu registered if no device_info set");
  }

  std::string req = "UPDATE device_info SET is_registered = 1";
  if (db.exec(req.c_str(), nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't set is_registered: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadEcuRegistered() {
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

bool SQLStorage::loadNeedReboot(bool* need_reboot) {
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

    if (!db.beginTransaction()) {
      LOG_ERROR << "Can't start transaction: " << db.errmsg();
      return;
    }

    if (db.exec("DELETE FROM ecu_serials;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't clear ecu_serials: " << db.errmsg();
      return;
    }

    // first is the primary
    std::string serial = serials[0].first.ToString();
    std::string hwid = serials[0].second.ToString();
    {
      auto statement = db.prepareStatement<std::string, std::string>(
          "INSERT INTO ecu_serials(id, serial,hardware_id,is_primary) VALUES (0, ?,?,1);", serial, hwid);
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
          "INSERT INTO ecu_serials(id,serial,hardware_id) VALUES (?,?,?);", it - serials.cbegin(), it->first.ToString(),
          it->second.ToString());

      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "Can't set ecu_serial: " << db.errmsg();
        return;
      }
    }

    db.commitTransaction();
  }
}

bool SQLStorage::loadEcuSerials(EcuSerials* serials) {
  SQLite3Guard db = dbConnection();

  // order by auto-incremented primary key so that the ecu order is kept constant
  auto statement = db.prepareStatement("SELECT serial, hardware_id FROM ecu_serials ORDER BY id;");
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
    LOG_ERROR << "Can't get ecu_serials: " << db.errmsg();
    return false;
  }

  if (serials != nullptr) {
    *serials = std::move(new_serials);
  }

  return !empty;
}

void SQLStorage::clearEcuSerials() {
  SQLite3Guard db = dbConnection();

  if (db.exec("DELETE FROM ecu_serials;", nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Can't clear ecu_serials: " << db.errmsg();
    return;
  }
}

void SQLStorage::storeMisconfiguredEcus(const std::vector<MisconfiguredEcu>& ecus) {
  if (ecus.size() >= 1) {
    SQLite3Guard db = dbConnection();

    if (!db.beginTransaction()) {
      LOG_ERROR << "Can't start transaction: " << db.errmsg();
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

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
    return;
  }

  // empty serial: use primary
  std::string ecu_serial_real = ecu_serial;
  if (ecu_serial_real.empty()) {
    auto statement = db.prepareStatement("SELECT serial FROM ecu_serials WHERE is_primary = 1;");
    if (statement.step() == SQLITE_ROW) {
      ecu_serial_real = statement.get_result_col_str(0).value();
    } else {
      LOG_WARNING << "Could not find primary ecu serial, set to lazy init mode";
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

  std::string hashes_encoded = Uptane::Hash::encodeVector(target.hashes());

  auto statement =
      db.prepareStatement<std::string, std::string, std::string, std::string, int64_t, std::string, int, int>(
          "INSERT OR REPLACE INTO installed_versions VALUES (?,?,?,?,?,?,?,?);", ecu_serial_real, target.sha256Hash(),
          target.filename(), hashes_encoded, static_cast<int64_t>(target.length()), target.correlation_id(),
          static_cast<int>(update_mode == InstalledVersionUpdateMode::kCurrent),
          static_cast<int>(update_mode == InstalledVersionUpdateMode::kPending));

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Can't set installed_versions: " << db.errmsg();
    return;
  }

  db.commitTransaction();
}

bool SQLStorage::loadInstalledVersions(const std::string& ecu_serial, std::vector<Uptane::Target>* installed_versions,
                                       size_t* current_version, size_t* pending_version) {
  SQLite3Guard db = dbConnection();

  // empty serial: use primary
  std::string ecu_serial_real = ecu_serial;
  if (ecu_serial_real.empty()) {
    auto statement = db.prepareStatement("SELECT serial FROM ecu_serials WHERE is_primary = 1;");
    if (statement.step() == SQLITE_ROW) {
      ecu_serial_real = statement.get_result_col_str(0).value();
    } else {
      LOG_WARNING << "Could not find primary ecu serial, defaulting to empty serial: " << db.errmsg();
    }
  }

  size_t current_index = SIZE_MAX;
  size_t pending_index = SIZE_MAX;
  auto statement = db.prepareStatement<std::string>(
      "SELECT sha256, name, hashes, length, correlation_id, is_current, is_pending FROM installed_versions WHERE "
      "ecu_serial = ?;",
      ecu_serial_real);
  int statement_state;

  std::vector<Uptane::Target> new_installed_versions;
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    try {
      auto sha256 = statement.get_result_col_str(0).value();
      auto filename = statement.get_result_col_str(1).value();
      auto hashes_str = statement.get_result_col_str(2).value();
      auto length = statement.get_result_col_int(3);
      auto correlation_id = statement.get_result_col_str(4).value();
      auto is_current = statement.get_result_col_int(5) != 0;
      auto is_pending = statement.get_result_col_int(6) != 0;

      // note: sha256 should always be present and is used to uniquely identify
      // a version. It should normally be part of the hash list as well.
      std::vector<Uptane::Hash> hashes = Uptane::Hash::decodeVector(hashes_str);

      auto find_sha256 = std::find_if(hashes.cbegin(), hashes.cend(),
                                      [](const Uptane::Hash& h) { return h.type() == Uptane::Hash::Type::kSha256; });
      if (find_sha256 == hashes.cend()) {
        LOG_WARNING << "No sha256 in hashes list";
        hashes.emplace_back(Uptane::Hash::Type::kSha256, sha256);
      }

      new_installed_versions.emplace_back(filename, hashes, length, correlation_id);
      if (is_current) {
        current_index = new_installed_versions.size() - 1;
      }
      if (is_pending) {
        pending_index = new_installed_versions.size() - 1;
      }
    } catch (const boost::bad_optional_access&) {
      LOG_ERROR << "Incompleted installed version, keeping old one";
      return false;
    }
  }

  if (statement_state != SQLITE_DONE) {
    LOG_ERROR << "Can't get installed_versions: " << db.errmsg();
    return false;
  }

  if (current_version != nullptr && current_index != SIZE_MAX) {
    *current_version = current_index;
  }

  if (pending_version != nullptr && pending_index != SIZE_MAX) {
    *pending_version = pending_index;
  }

  if (installed_versions != nullptr) {
    *installed_versions = std::move(new_installed_versions);
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
    LOG_ERROR << "Can't set ecu installation result: " << db.errmsg();
    return;
  }
}

bool SQLStorage::loadEcuInstallationResults(
    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>>* results) {
  SQLite3Guard db = dbConnection();

  std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_res;

  // keep the same order as in ecu_serials (start with primary)
  auto statement = db.prepareStatement(
      "SELECT ecu_serial, success, result_code, description FROM ecu_installation_results INNER JOIN ecu_serials ON "
      "ecu_serials.serial = ecu_serial ORDER BY ecu_serials.id;");
  int statement_result = statement.step();
  if (statement_result != SQLITE_DONE && statement_result != SQLITE_ROW) {
    LOG_ERROR << "Can't get ecu_installation_results: " << db.errmsg();
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

bool SQLStorage::loadDeviceInstallationResult(data::InstallationResult* result, std::string* raw_report,
                                              std::string* correlation_id) {
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

void SQLStorage::clearInstallationResults() {
  SQLite3Guard db = dbConnection();
  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
    return;
  }

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

boost::optional<std::pair<size_t, std::string>> SQLStorage::checkTargetFile(const Uptane::Target& target) const {
  SQLite3Guard db = dbConnection();

  auto statement = db.prepareStatement<std::string>(
      "SELECT real_size, sha256, sha512, filename FROM target_images WHERE targetname = ?;", target.filename());

  int statement_state;
  while ((statement_state = statement.step()) == SQLITE_ROW) {
    auto sha256 = statement.get_result_col_str(1);
    auto sha512 = statement.get_result_col_str(2);
    if ((*sha256).empty() && (*sha512).empty()) {
      // Old aktualizr didn't save checksums, this could require to redownload old images.
      LOG_WARNING << "Image without checksum: " << target.filename();
      continue;
    }
    bool sha256_match = false;
    bool sha512_match = false;
    if (!(*sha256).empty()) {
      if (target.MatchWith(Uptane::Hash(Uptane::Hash::Type::kSha256, *sha256))) {
        sha256_match = true;
      }
    }

    if (!(*sha512).empty()) {
      if (target.MatchWith(Uptane::Hash(Uptane::Hash::Type::kSha512, *sha512))) {
        sha512_match = true;
      }
    }
    if (((*sha256).empty() || sha256_match) && ((*sha512).empty() || sha512_match)) {
      if (boost::filesystem::exists(images_path_ / *statement.get_result_col_str(3))) {
        return {{static_cast<size_t>(statement.get_result_col_int(0)), *statement.get_result_col_str(3)}};
      } else {
        return boost::none;
      }
    }
  }

  if (statement_state == SQLITE_DONE) {
    LOG_INFO << "No file '" + target.filename() << "' with matched hash in the database";
    return boost::none;
  }
  assert(statement_state != SQLITE_ROW);  // from the previous loop precondition
  LOG_ERROR << "Statement step failure: " << db.errmsg();

  return boost::none;
}

class SQLTargetWHandle : public StorageTargetWHandle {
 public:
  SQLTargetWHandle(const SQLStorage& storage, Uptane::Target target)
      : db_(storage.dbPath()), target_(std::move(target)) {
    StorageTargetWHandle::WriteError exc("could not save file " + target_.filename() + " to the filesystem");

    std::string sha256Hash;
    std::string sha512Hash;
    for (const auto& hash : target_.hashes()) {
      if (hash.type() == Uptane::Hash::Type::kSha256) {
        sha256Hash = hash.HashString();
      } else if (hash.type() == Uptane::Hash::Type::kSha512) {
        sha512Hash = hash.HashString();
      }
    }
    std::string filename = (storage.images_path_ / target_.hashes()[0].HashString()).string();
    auto statement = db_.prepareStatement<std::string, std::string, std::string>(
        "INSERT OR REPLACE INTO target_images (targetname, sha256, sha512, filename) VALUES ( ?, ?, ?, ?);",
        target_.filename(), sha256Hash, sha512Hash, target_.hashes()[0].HashString());

    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Statement step failure: " << db_.errmsg();
      throw exc;
    }
    boost::filesystem::create_directories(storage.images_path_);
    stream_.open(filename);
    if (!stream_.good()) {
      LOG_ERROR << "Could not open image for write: " << storage.images_path_ / target_.filename();
      throw exc;
    }
  }

  ~SQLTargetWHandle() override { SQLTargetWHandle::wcommit(); }

  size_t wfeed(const uint8_t* buf, size_t size) override {
    stream_.write(reinterpret_cast<const char*>(buf), static_cast<std::streamsize>(size));
    written_size_ += size;

    return size;
  }

  void wcommit() override {
    if (stream_) {
      stream_.close();
      auto statement =
          db_.prepareStatement<int64_t, std::string>("UPDATE target_images SET real_size = ? WHERE targetname = ?;",
                                                     static_cast<int64_t>(written_size_), target_.filename());

      int err = statement.step();
      if (err != SQLITE_DONE) {
        LOG_ERROR << "Could not save size in db: " << db_.errmsg();
        throw StorageTargetWHandle::WriteError("could not update size of " + target_.filename() + " in sql storage");
      }
    }
  }

  void wabort() noexcept override {
    if (stream_) {
      stream_.close();
    }
    if (sqlite3_changes(db_.get()) > 0) {
      auto statement =
          db_.prepareStatement<std::string>("DELETE FROM target_images WHERE targetname=?;", target_.filename());
      if (statement.step() != SQLITE_DONE) {
        LOG_ERROR << "could not delete " << target_.filename() << " from sql storage";
      }
    }
  }

  friend class SQLTargetRHandle;

 private:
  SQLTargetWHandle(const boost::filesystem::path& db_path, Uptane::Target target,
                   const boost::filesystem::path& image_path, const size_t& start_from = 0)
      : db_(db_path), target_(std::move(target)) {
    if (db_.get_rc() != SQLITE_OK) {
      LOG_ERROR << "Can't open database: " << db_.errmsg();
      throw StorageTargetWHandle::WriteError("could not open sql storage");
    }
    stream_.open(image_path.string(), std::ofstream::out | std::ofstream::app);
    if (!stream_.good()) {
      LOG_ERROR << "Could not open image for write: " << image_path;
      throw StorageTargetWHandle::WriteError("could not open file for write: " + image_path.string());
    }

    written_size_ = start_from;
  }
  SQLite3Guard db_;
  Uptane::Target target_;
  std::ofstream stream_;
};

std::unique_ptr<StorageTargetWHandle> SQLStorage::allocateTargetFile(bool from_director, const Uptane::Target& target) {
  (void)from_director;
  return std::unique_ptr<StorageTargetWHandle>(new SQLTargetWHandle(*this, target));
}

class SQLTargetRHandle : public StorageTargetRHandle {
 public:
  SQLTargetRHandle(const SQLStorage& storage, Uptane::Target target)
      : db_path_(storage.dbPath()), db_(db_path_), target_(std::move(target)), size_(0) {
    StorageTargetRHandle::ReadError exc("could not read file " + target_.filename() + " from sql storage");

    auto exists = storage.checkTargetFile(target_);
    if (!exists) {
      throw exc;
    }

    size_ = exists->first;
    partial_ = size_ < target_.length();
    image_path_ = storage.images_path_ / exists->second;
    stream_.open(image_path_.string());
    if (!stream_.good()) {
      LOG_ERROR << "Could not open image: " << storage.images_path_ / target_.filename();
      throw exc;
    }
  }

  ~SQLTargetRHandle() override { SQLTargetRHandle::rclose(); }

  size_t rsize() const override { return size_; }

  size_t rread(uint8_t* buf, size_t size) override {
    stream_.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(size));
    return static_cast<size_t>(stream_.gcount());
  }

  void rclose() noexcept override {
    if (stream_.is_open()) {
      stream_.close();
    }
  }

  bool isPartial() const noexcept override { return partial_; }
  std::unique_ptr<StorageTargetWHandle> toWriteHandle() override {
    return std::unique_ptr<StorageTargetWHandle>(new SQLTargetWHandle(db_path_, target_, image_path_, size_));
  }

 private:
  boost::filesystem::path db_path_;
  SQLite3Guard db_;
  Uptane::Target target_;
  size_t size_;
  bool partial_{false};
  boost::filesystem::path image_path_;
  std::ifstream stream_;
};

std::unique_ptr<StorageTargetRHandle> SQLStorage::openTargetFile(const Uptane::Target& target) {
  return std_::make_unique<SQLTargetRHandle>(*this, target);
}

void SQLStorage::removeTargetFile(const std::string& filename) {
  SQLite3Guard db = dbConnection();

  if (!db.beginTransaction()) {
    LOG_ERROR << "Can't start transaction: " << db.errmsg();
    return;
  }

  auto statement =
      db.prepareStatement<std::string>("SELECT targetname FROM target_images WHERE targetname = ?;", filename);

  if (statement.step() != SQLITE_ROW) {
    LOG_ERROR << "Statement step failure: " << db.errmsg();
    throw std::runtime_error("Could not find target file");
  }

  statement = db.prepareStatement<std::string>("DELETE FROM target_images WHERE targetname=?;", filename);

  if (statement.step() != SQLITE_DONE) {
    LOG_ERROR << "Statement step failure: " << db.errmsg();
    throw std::runtime_error("Could not remove target file");
  }
  boost::filesystem::remove(images_path_ / filename);

  db.commitTransaction();
}

void SQLStorage::cleanUp() { boost::filesystem::remove_all(dbPath()); }
