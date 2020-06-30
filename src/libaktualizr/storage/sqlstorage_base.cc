#include <sys/stat.h>

#include "sqlstorage_base.h"
#include "storage_exception.h"
#include "utilities/utils.h"

boost::filesystem::path SQLStorageBase::dbPath() const { return sqldb_path_; }

StorageLock::StorageLock(boost::filesystem::path path) : lock_path(std::move(path)) {
  {
    std::fstream fs;
    fs.open(lock_path.c_str(), std::fstream::in | std::fstream::out | std::fstream::app);
  }
  fl_ = lock_path.c_str();
  if (!fl_.try_lock()) {
    throw StorageLock::locked_exception();
  }
}

StorageLock::~StorageLock() {
  try {
    if (!lock_path.empty()) {
      fl_.unlock();
      std::remove(lock_path.c_str());
    }
  } catch (std::exception& e) {
  }
}

SQLStorageBase::SQLStorageBase(boost::filesystem::path sqldb_path, bool readonly,
                               std::vector<std::string> schema_migrations,
                               std::vector<std::string> schema_rollback_migrations, std::string current_schema,
                               int current_schema_version)
    : sqldb_path_(std::move(sqldb_path)),
      readonly_(readonly),
      mutex_(new std::mutex()),
      schema_migrations_(std::move(schema_migrations)),
      schema_rollback_migrations_(std::move(schema_rollback_migrations)),
      current_schema_(std::move(current_schema)),
      current_schema_version_(current_schema_version) {
  boost::filesystem::path db_parent_path = dbPath().parent_path();
  if (!boost::filesystem::is_directory(db_parent_path)) {
    Utils::createDirectories(db_parent_path, S_IRWXU);
  } else {
    struct stat st {};
    if (stat(db_parent_path.c_str(), &st) < 0) {
      throw StorageException(std::string("Could not check storage directory permissions: ") + std::strerror(errno));
    }
    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
      throw StorageException("Storage directory has unsafe permissions");
    }
    if ((st.st_mode & (S_IRGRP | S_IROTH)) != 0) {
      // Remove read permissions for group and others
      if (chmod(db_parent_path.c_str(), S_IRWXU) < 0) {
        throw StorageException("Storage directory has unsafe permissions");
      }
    }
  }

  if (!readonly) {
    try {
      lock = StorageLock(db_parent_path / "storage.lock");
    } catch (StorageLock::locked_exception& e) {
      LOG_WARNING << "\033[31m"
                  << "Storage in " << db_parent_path
                  << " is already in use, running several instances concurrently may result in data corruption!"
                  << "\033[0m";
    }
  }

  if (!dbMigrate()) {
    throw StorageException("SQLite database migration failed");
  }
}

SQLite3Guard SQLStorageBase::dbConnection() const {
  SQLite3Guard db(dbPath(), readonly_, mutex_);
  if (db.get_rc() != SQLITE_OK) {
    throw SQLException(std::string("Can't open database: ") + db.errmsg());
  }
  return db;
}

std::string SQLStorageBase::getTableSchemaFromDb(const std::string& tablename) {
  SQLite3Guard db = dbConnection();

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

bool SQLStorageBase::dbInsertBackMigrations(SQLite3Guard& db, int version_latest) {
  if (schema_rollback_migrations_.empty()) {
    LOG_TRACE << "No backward migrations defined";
    return true;
  }

  if (schema_rollback_migrations_.size() < static_cast<size_t>(version_latest) + 1) {
    LOG_ERROR << "Backward migrations from " << schema_rollback_migrations_.size() << " to " << version_latest
              << " are missing";
    return false;
  }

  for (int k = 1; k <= version_latest; k++) {
    if (schema_rollback_migrations_.at(static_cast<size_t>(k)).empty()) {
      continue;
    }
    auto statement = db.prepareStatement("INSERT OR REPLACE INTO rollback_migrations VALUES (?,?);", k,
                                         schema_rollback_migrations_.at(static_cast<uint32_t>(k)));
    if (statement.step() != SQLITE_DONE) {
      LOG_ERROR << "Can't insert rollback migration script: " << db.errmsg();
      return false;
    }
  }

  return true;
}

bool SQLStorageBase::dbMigrateForward(int version_from, int version_to) {
  if (version_to <= 0) {
    version_to = current_schema_version_;
  }

  LOG_INFO << "Migrating DB from version " << version_from << " to version " << version_to;

  SQLite3Guard db = dbConnection();

  try {
    db.beginTransaction();
  } catch (const SQLException& e) {
    return false;
  }

  for (int32_t k = version_from + 1; k <= version_to; k++) {
    auto result_code = db.exec(schema_migrations_.at(static_cast<size_t>(k)), nullptr, nullptr);
    if (result_code != SQLITE_OK) {
      LOG_ERROR << "Can't migrate DB from version " << (k - 1) << " to version " << k << ": " << db.errmsg();
      return false;
    }
  }

  if (!dbInsertBackMigrations(db, version_to)) {
    return false;
  }

  db.commitTransaction();

  return true;
}

bool SQLStorageBase::dbMigrateBackward(int version_from, int version_to) {
  if (version_to <= 0) {
    version_to = current_schema_version_;
  }

  LOG_INFO << "Migrating DB backward from version " << version_from << " to version " << version_to;

  SQLite3Guard db = dbConnection();
  for (int ver = version_from; ver > version_to; --ver) {
    std::string migration;
    {
      // make sure the statement is destroyed before the next database operation
      auto statement = db.prepareStatement("SELECT migration FROM rollback_migrations WHERE version_from=?;", ver);
      if (statement.step() != SQLITE_ROW) {
        LOG_ERROR << "Can't extract migration script: " << db.errmsg();
        return false;
      }
      migration = *(statement.get_result_col_str(0));
    }

    if (db.exec(migration, nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't migrate db from version " << (ver) << " to version " << ver - 1 << ": " << db.errmsg();
      return false;
    }
    if (db.exec(std::string("DELETE FROM rollback_migrations WHERE version_from=") + std::to_string(ver) + ";", nullptr,
                nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't clear old migration script: " << db.errmsg();
      return false;
    }
  }
  return true;
}

bool SQLStorageBase::dbMigrate() {
  DbVersion schema_version = getVersion();

  if (schema_version == DbVersion::kInvalid) {
    LOG_ERROR << "Sqlite database file is invalid.";
    return false;
  }

  auto schema_num_version = static_cast<int32_t>(schema_version);
  if (schema_num_version == current_schema_version_) {
    return true;
  }

  if (readonly_) {
    LOG_ERROR << "Database is opened in readonly mode and cannot be migrated to latest version";
    return false;
  }

  if (schema_version == DbVersion::kEmpty) {
    LOG_INFO << "Bootstraping DB to version " << current_schema_version_;
    SQLite3Guard db = dbConnection();

    try {
      db.beginTransaction();
    } catch (const SQLException& e) {
      return false;
    }

    auto result_code = db.exec(current_schema_, nullptr, nullptr);
    if (result_code != SQLITE_OK) {
      LOG_ERROR << "Can't bootstrap DB to version " << current_schema_version_ << ": " << db.errmsg();
      return false;
    }

    if (!dbInsertBackMigrations(db, current_schema_version_)) {
      return false;
    }

    db.commitTransaction();
  } else if (schema_num_version > current_schema_version_) {
    dbMigrateBackward(schema_num_version);
  } else {
    dbMigrateForward(schema_num_version);
  }

  return true;
}

DbVersion SQLStorageBase::getVersion() {
  SQLite3Guard db = dbConnection();

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
  } catch (const SQLException&) {
    return DbVersion::kInvalid;
  }
}
