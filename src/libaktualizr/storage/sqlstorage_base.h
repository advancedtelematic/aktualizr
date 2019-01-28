#ifndef SQLSTORAGE_BASE_H_
#define SQLSTORAGE_BASE_H_

#include <boost/filesystem.hpp>

#include "sql_utils.h"
#include "storage_config.h"

enum class DbVersion : int32_t { kEmpty = -1, kInvalid = -2 };

class SQLStorageBase {
 public:
  explicit SQLStorageBase(boost::filesystem::path sqldb_path, bool readonly, std::vector<std::string> schema_migrations,
                          std::string current_schema, int current_schema_version);
  ~SQLStorageBase() = default;
  std::string getTableSchemaFromDb(const std::string& tablename);
  bool dbMigrate();
  DbVersion getVersion();  // non-negative integer on success or -1 on error
  boost::filesystem::path dbPath() const;

 protected:
  SQLite3Guard dbConnection() const;
  boost::filesystem::path sqldb_path_;
  bool readonly_{false};
  const std::vector<std::string> schema_migrations_;
  const std::string current_schema_;
  const int current_schema_version_;
};

#endif  // SQLSTORAGE_BASE_H_
