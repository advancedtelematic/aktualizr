#ifndef SQL_UTILS_H_
#define SQL_UTILS_H_

#include <list>
#include <map>
#include <memory>
#include <mutex>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <sqlite3.h>

#include "logging/logging.h"
#include "utilities/utils.h"

// Unique ownership SQLite3 statement creation

struct SQLBlob {
  const std::string& content;
  explicit SQLBlob(const std::string& str) : content(str) {}
};

struct SQLZeroBlob {
  size_t size;
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
    auto b = reinterpret_cast<const char*>(sqlite3_column_blob(stmt_.get(), iCol));
    if (b == nullptr) {
      return boost::none;
    }
    return std::string(b);
  }

  inline boost::optional<std::string> get_result_col_str(int iCol) {
    auto b = reinterpret_cast<const char*>(sqlite3_column_text(stmt_.get(), iCol));
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

  void bindArgument(const SQLZeroBlob& blob) {
    if (sqlite3_bind_zeroblob(stmt_.get(), bind_cnt_, static_cast<int>(blob.size)) != SQLITE_OK) {
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

class SQLite3Transaction;

// Unique ownership SQLite3 connection
class SQLite3Guard {
 public:
  sqlite3* get() { return handle_.get(); }
  int get_rc() { return rc_; }

  explicit SQLite3Guard(const char* path, bool readonly)
      : handle_(nullptr, sqlite3_close), rc_(0), path_(std::string(path)) {
    if (sqlite3_threadsafe() == 0) {
      throw std::runtime_error("sqlite3 has been compiled without multitheading support");
    }

    sqlite3* h;
    if (readonly) {
      rc_ = sqlite3_open_v2(path_.c_str(), &h, SQLITE_OPEN_READONLY, nullptr);
    } else {
      rc_ =
          sqlite3_open_v2(path_.c_str(), &h, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    }
    handle_.reset(h);
  }

  explicit SQLite3Guard(const boost::filesystem::path& path, bool readonly = false)
      : SQLite3Guard(path.c_str(), readonly) {}
  SQLite3Guard(SQLite3Guard&& guard) noexcept : handle_(std::move(guard.handle_)), rc_(guard.rc_) {}
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

  std::unique_ptr<SQLite3Transaction> makeTransaction() {
    try {
      return std_::make_unique<SQLite3Transaction>(*this);
    } catch (...) {
      return std::unique_ptr<SQLite3Transaction>(nullptr);
    }
  }

  std::string errmsg() const { return sqlite3_errmsg(handle_.get()); }

  std::string path() const { return path_; }

 private:
  std::unique_ptr<sqlite3, int (*)(sqlite3*)> handle_;
  int rc_;
  std::string path_;
};

class SQLite3Transaction {
 public:
  SQLite3Transaction(SQLite3Guard& guard) : guard_(guard) {
    // Note: transaction cannot be nested and this will fail if another
    // transaction was open on the same connection
    boost::filesystem::path db_path = boost::filesystem::weakly_canonical(boost::filesystem::path(guard_.path()));
    std::mutex* path_mutex;
    {
      std::lock_guard<std::mutex> lk(db_locks_mutex);
      path_mutex = &(db_locks[db_path]);  // [] on map will also default-construct a mutex if it's not there yet
    }
    std::unique_lock<std::mutex> ul(*path_mutex);
    std::swap(lock_, ul);

    int ret = guard_.exec("BEGIN TRANSACTION;", nullptr, nullptr);
    if (ret != SQLITE_OK) {
      throw std::runtime_error(std::string("Can't begin transaction: ") + guard_.errmsg());
    }
  }

  ~SQLite3Transaction() {
    if (lock_.owns_lock()) {  // haven't committed yet, rollback the transaction
      guard_.exec("ROLLBACK TRANSACTION;", nullptr, nullptr);
    }
  }

  bool commit() {
    int ret = guard_.exec("COMMIT TRANSACTION;", nullptr, nullptr);
    if (ret != SQLITE_OK) {
      LOG_ERROR << "Can't commit transaction: " << guard_.errmsg();
    }
    lock_.unlock();
    lock_.release();
    return ret == SQLITE_OK;
  }

 private:
  std::unique_lock<std::mutex> lock_;
  SQLite3Guard& guard_;
  static std::map<boost::filesystem::path, std::mutex> db_locks;
  static std::mutex db_locks_mutex;
};

#endif  // SQL_UTILS_H_
