#ifndef SQL_UTILS_H_
#define SQL_UTILS_H_

#include <list>
#include <memory>

#include <sqlite3.h>

#include "logging.h"

// Unique ownership SQLite3 statement creation

struct SQLBlob {
  const std::string& content;
  SQLBlob(const std::string& str) : content(str) {}
};

struct SQLZeroBlob {
  size_t size;
};

class SQLiteStatement {
 public:
  template <typename... Types>
  SQLiteStatement(sqlite3* db, const std::string& zSql, const Types&... args)
      : db_(db), stmt_(nullptr, sqlite3_finalize), bind_cnt_(1) {
    sqlite3_stmt* statement;

    if (sqlite3_prepare_v2(db_, zSql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Could not prepare statement: " << sqlite3_errmsg(db_);
      throw std::runtime_error("SQLite fatal error");
    }
    stmt_.reset(statement);

    bindArguments(args...);
  }

  inline sqlite3_stmt* get() const { return stmt_.get(); }
  inline int step() const { return sqlite3_step(stmt_.get()); }

 private:
  void bindArgument(int v) {
    if (sqlite3_bind_int(stmt_.get(), bind_cnt_, v) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_);
      throw std::runtime_error("SQLite bind error");
    }
  }

  void bindArgument(int64_t v) {
    if (sqlite3_bind_int64(stmt_.get(), bind_cnt_, v) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_);
      throw std::runtime_error("SQLite bind error");
    }
  }

  void bindArgument(const std::string& v) {
    owned_data_.push_back(v);
    const std::string& oe = owned_data_.back();

    if (sqlite3_bind_text(stmt_.get(), bind_cnt_, oe.c_str(), -1, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_);
      throw std::runtime_error("SQLite bind error");
    }
  }

  void bindArgument(const char* v) { bindArgument(std::string(v)); }

  void bindArgument(const SQLBlob& blob) {
    owned_data_.emplace_back(blob.content);
    const std::string& oe = owned_data_.back();

    if (sqlite3_bind_blob(stmt_.get(), bind_cnt_, oe.c_str(), oe.size(), SQLITE_STATIC) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_);
      throw std::runtime_error("SQLite bind error");
    }
  }

  void bindArgument(const SQLZeroBlob& blob) {
    if (sqlite3_bind_zeroblob(stmt_.get(), bind_cnt_, blob.size) != SQLITE_OK) {
      LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db_);
      throw std::runtime_error("SQLite bind error");
    }
  }

  /* end of template specialization */
  void bindArguments() {}

  template <typename T, typename... Types>
  void bindArguments(const T& v, const Types&... args) {
    bindArgument(v);
    bind_cnt_ += 1;
    bindArguments(args...);
  }

  sqlite3* db_;
  std::unique_ptr<sqlite3_stmt, int (*)(sqlite3_stmt*)> stmt_;
  int bind_cnt_;
  // copies of data that need to persist for the object duration
  // (avoid vector because of resizing issues)
  std::list<std::string> owned_data_;
};

// Unique ownership SQLite3 connection
class SQLite3Guard {
 public:
  sqlite3* get() { return handle_.get(); }
  int get_rc() { return rc_; }

  SQLite3Guard(const char* path) : handle_(nullptr, sqlite3_close), rc_(0) {
    sqlite3* h;
    rc_ = sqlite3_open(path, &h);
    handle_.reset(h);
  }

  int exec(const char* sql, int (*callback)(void*, int, char**, char**), void* cb_arg) {
    return sqlite3_exec(handle_.get(), sql, callback, cb_arg, NULL);
  }

  template <typename... Types>
  SQLiteStatement prepareStatement(const std::string& zSql, const Types&... args) {
    return SQLiteStatement(handle_.get(), zSql, args...);
  }

  std::string errmsg() const { return sqlite3_errmsg(handle_.get()); }

  // Transaction handling
  //
  // A transactional series of db operations should be realized between calls of
  // `beginTranscation()` and `commitTransaction()`. If no commit is done before
  // the destruction of the `SQLite3Guard` (and thus the SQLite connection) or
  // if `rollbackTransaction()` is called explicitely, the changes will be
  // rolled back

  bool beginTransaction() {
    // Note: transaction cannot be nested and this will fail if another
    // transaction was open on the same connection
    int ret = exec("BEGIN TRANSACTION;", NULL, NULL);
    if (ret != SQLITE_OK) {
      LOG_ERROR << "Can't begin transaction: " << errmsg();
    }
    return ret == SQLITE_OK;
  }

  bool commitTransaction() {
    int ret = exec("COMMIT TRANSACTION;", NULL, NULL);
    if (ret != SQLITE_OK) {
      LOG_ERROR << "Can't commit transaction: " << errmsg();
    }
    return ret == SQLITE_OK;
  }

  bool rollbackTransaction() {
    int ret = exec("ROLLBACK TRANSACTION;", NULL, NULL);
    if (ret != SQLITE_OK) {
      LOG_ERROR << "Can't rollback transaction: " << errmsg();
    }
    return ret == SQLITE_OK;
  }

 private:
  std::unique_ptr<sqlite3, int (*)(sqlite3*)> handle_;
  int rc_;
};

#endif  // SQL_UTILS_H_
