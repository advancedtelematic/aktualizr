#ifndef SQLSTORAGE_BASE_H_
#define SQLSTORAGE_BASE_H_

#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <libaktualizr/storage_config.h>

#include "sql_utils.h"

enum class DbVersion : int32_t { kEmpty = -1, kInvalid = -2 };

class StorageLock {
 public:
  StorageLock() = default;
  StorageLock(boost::filesystem::path path);
  StorageLock(StorageLock &other) = delete;
  StorageLock &operator=(StorageLock &other) = delete;
  StorageLock(StorageLock &&other) = default;
  StorageLock &operator=(StorageLock &&other) = default;
  virtual ~StorageLock();

  class locked_exception : std::runtime_error {
   public:
    locked_exception() : std::runtime_error("locked") {}
  };

 private:
  boost::filesystem::path lock_path;
  boost::interprocess::file_lock fl_;
};

class SQLStorageBase {
 public:
  explicit SQLStorageBase(boost::filesystem::path sqldb_path, bool readonly, std::vector<std::string> schema_migrations,
                          std::vector<std::string> schema_rollback_migrations, std::string current_schema,
                          int current_schema_version);
  ~SQLStorageBase() = default;
  std::string getTableSchemaFromDb(const std::string &tablename);
  bool dbMigrateForward(int version_from, int version_to = 0);
  bool dbMigrateBackward(int version_from, int version_to = 0);
  bool dbMigrate();
  DbVersion getVersion();  // non-negative integer on success or -1 on error
  boost::filesystem::path dbPath() const;

 protected:
  boost::filesystem::path sqldb_path_;
  bool readonly_{false};

  StorageLock lock;
  std::shared_ptr<std::mutex> mutex_;

  const std::vector<std::string> schema_migrations_;
  std::vector<std::string> schema_rollback_migrations_;
  const std::string current_schema_;
  const int current_schema_version_;

  SQLite3Guard dbConnection() const;
  bool dbInsertBackMigrations(SQLite3Guard &db, int version_latest);
};

#endif  // SQLSTORAGE_BASE_H_
