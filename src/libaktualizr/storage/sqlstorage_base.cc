#include "sqlstorage_base.h"
#include "storage_exception.h"

#include <sys/stat.h>

boost::filesystem::path SQLStorageBase::dbPath() const { return sqldb_path_; }

SQLStorageBase::SQLStorageBase(boost::filesystem::path sqldb_path, bool readonly,
                               std::vector<std::string> schema_migrations, std::string current_schema,
                               int current_schema_version)
    : sqldb_path_(std::move(sqldb_path)),
      readonly_(readonly),
      schema_migrations_(std::move(schema_migrations)),
      current_schema_(std::move(current_schema)),
      current_schema_version_(current_schema_version) {
  boost::filesystem::path db_parent_path = dbPath().parent_path();
  if (!boost::filesystem::is_directory(db_parent_path)) {
    Utils::createDirectories(db_parent_path, S_IRWXU);
  } else {
    struct stat st {};
    stat(db_parent_path.c_str(), &st);
    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
      throw StorageException("Storage directory has unsafe permissions");
    }
    if ((st.st_mode & (S_IRGRP | S_IROTH)) != 0) {
      // Remove read permissions for group and others
      chmod(db_parent_path.c_str(), S_IRWXU);
    }
  }

  if (!dbMigrate()) {
    LOG_ERROR << "SQLite database migration failed";
    // Continue to run anyway, it can't be worse
  }
}

SQLite3Guard SQLStorageBase::dbConnection() const {
  SQLite3Guard db(dbPath(), readonly_);
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

  if (schema_num_version > current_schema_version_) {
    LOG_ERROR << "Only forward migrations are supported. You cannot migrate to an older schema.";
    return false;
  }

  SQLite3Guard db = dbConnection();
  for (int32_t k = schema_num_version + 1; k <= current_schema_version_; k++) {
    if (db.exec(schema_migrations_.at(static_cast<size_t>(k)), nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't migrate db from version " << (k - 1) << " to version " << k << ": " << db.errmsg();
      return false;
    }
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
