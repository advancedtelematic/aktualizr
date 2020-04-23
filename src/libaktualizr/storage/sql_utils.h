#ifndef SQL_UTILS_H_
#define SQL_UTILS_H_

#include <list>
#include <memory>
#include <mutex>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <sqlite3.h>

#include "logging/logging.h"

// Unique ownership SQLite3 statement creation

struct SQLBlob {
  const std::string& content;
  explicit SQLBlob(const std::string& str) : content(str) {}
};

class SQLException : public std::runtime_error {
 public:
  SQLException(const std::string& what = "SQL error") : std::runtime_error(what) {}
  ~SQLException() noexcept override = default;
};

class SQLiteStatement {
 public:
  template <typename... Types>
  SQLiteStatement(sqlite3* db, const std::string& zSql, const Types&... args)
      : db_(db), stmt_(nullptr, sqlite3_finalize), bind_cnt_(1) {
    sqlite3_stmt* statement;

    if (sqlite3_prepare_v2(db_, zSql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Could not prepare statement: " << sqlite3_errmsg(db_);
      throw SQLException();
    }
    stmt_.reset(statement);

    bindArguments(args...);
  }

  inline sqlite3_stmt* get() const { return stmt_.get(); }
  inline int step() const { return sqlite3_step(stmt_.get()); }

  // get results
  inline boost::optional<std::string> get_result_col_blob(int iCol) {
    const auto* b = reinterpret_cast<const char*>(sqlite3_column_blob(stmt_.get(), iCol));
    if (b == nullptr) {
      return boost::none;
    }
    auto length = static_cast<size_t>(sqlite3_column_bytes(stmt_.get(), iCol));
    return std::string(b, length);
  }

  inline boost::optional<std::string> get_result_col_str(int iCol) {
    const auto* b = reinterpret_cast<const char*>(sqlite3_column_text(stmt_.get(), iCol));
    if (b == nullptr) {
      return boost::none;
    }
    return std::string(b);
  }

  inline int64_t get_result_col_int(int iCol) { return sqlite3_column_int64(stmt_.get(), iCol); }

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

    if (sqlite3_bind_blob(stmt_.get(), bind_cnt_, oe.c_str(), static_cast<int>(oe.size()), SQLITE_STATIC) !=
        SQLITE_OK) {
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
extern std::mutex sql_mutex;
class SQLite3Guard {
 public:
  sqlite3* get() { return handle_.get(); }
  int get_rc() const { return rc_; }

  explicit SQLite3Guard(const char* path, bool readonly, std::shared_ptr<std::mutex> mutex = nullptr)
      : handle_(nullptr, sqlite3_close), rc_(0), m_(std::move(mutex)) {
    if (m_) {
      m_->lock();
    }
    if (sqlite3_threadsafe() == 0) {
      throw std::runtime_error("sqlite3 has been compiled without multitheading support");
    }
    sqlite3* h;
    if (readonly) {
      rc_ = sqlite3_open_v2(path, &h, SQLITE_OPEN_READONLY, nullptr);
    } else {
      rc_ = sqlite3_open_v2(path, &h, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    }

    /* retry operations for 2 seconds before returning SQLITE_BUSY */
    sqlite3_busy_timeout(h, 2000);

    handle_.reset(h);
  }

  explicit SQLite3Guard(const boost::filesystem::path& path, bool readonly = false,
                        std::shared_ptr<std::mutex> mutex = nullptr)
      : SQLite3Guard(path.c_str(), readonly, std::move(mutex)) {}
  SQLite3Guard(SQLite3Guard&& guard) noexcept : handle_(std::move(guard.handle_)), rc_(guard.rc_) {}
  ~SQLite3Guard() {
    if (m_) {
      m_->unlock();
    }
  }
  SQLite3Guard(const SQLite3Guard& guard) = delete;
  SQLite3Guard operator=(const SQLite3Guard& guard) = delete;

  int exec(const char* sql, int (*callback)(void*, int, char**, char**), void* cb_arg) {
    return sqlite3_exec(handle_.get(), sql, callback, cb_arg, nullptr);
  }

  int exec(const std::string& sql, int (*callback)(void*, int, char**, char**), void* cb_arg) {
    return exec(sql.c_str(), callback, cb_arg);
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

  void beginTransaction() {
    // Note: transaction cannot be nested and this will fail if another
    // transaction was open on the same connection
    if (exec("BEGIN TRANSACTION;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't begin transaction: " << errmsg();
      throw SQLException("Transaction error");
    }
  }

  void commitTransaction() {
    if (exec("COMMIT TRANSACTION;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't commit transaction: " << errmsg();
      throw SQLException("Transaction error");
    }
  }

  void rollbackTransaction() {
    if (exec("ROLLBACK TRANSACTION;", nullptr, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Can't rollback transaction: " << errmsg();
      throw SQLException("Transaction error");
    }
  }

 private:
  std::unique_ptr<sqlite3, int (*)(sqlite3*)> handle_;
  int rc_;
  std::shared_ptr<std::mutex> m_ = nullptr;
};

#endif  // SQL_UTILS_H_
