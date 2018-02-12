#ifndef SQL_UTILS_H_
#define SQL_UTILS_H_

#include <memory>

#include <sqlite3.h>

template <typename T>
static void bindArguments(sqlite3* db, sqlite3_stmt* statement, int cnt, int v) {
  if (sqlite3_bind_int(statement, cnt, v) != SQLITE_OK) {
    LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db);
    throw std::runtime_error("SQLite bind error");
  }
}

template <typename T>
static void bindArgument(sqlite3* db, sqlite3_stmt* statement, int cnt, const char* v) {
  if (sqlite3_bind_text(statement, cnt, v, -1, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db);
    throw std::runtime_error("SQLite bind error");
  }
}

struct SQLZeroBlob {
  size_t size;
};

template <typename T>
static void bindArgument(sqlite3* db, sqlite3_stmt* statement, int cnt, SQLZeroBlob& blob) {
  if (sqlite3_bind_zeroblob(statement, cnt, blob.size) != SQLITE_OK) {
    LOG_ERROR << "Could not bind: " << sqlite3_errmsg(db);
    throw std::runtime_error("SQLite bind error");
  }
}

template <typename... Types>
static void bindArguments(sqlite3* db, sqlite3_stmt* statement, int cnt) {
  /* end of template specialization */
  (void)db;
  (void)statement;
  (void)cnt;
}

template <typename T, typename... Types>
static void bindArguments(sqlite3* db, sqlite3_stmt* statement, int cnt, T& v, Types... args) {
  bindArgument<T>(db, statement, cnt, v);
  bindArguments<Types...>(db, statement, cnt + 1, args...);
}

class SQLStatement {
 public:
  inline sqlite3_stmt* get() const { return stmt_.get(); }
  inline int step() const { return sqlite3_step(stmt_.get()); }

  template <typename... Types>
  static SQLStatement prepare(sqlite3* db, const char* zSql, Types... args) {
    sqlite3_stmt* statement;

    if (sqlite3_prepare_v2(db, zSql, -1, &statement, nullptr) != SQLITE_OK) {
      LOG_ERROR << "Could not prepare statement: " << sqlite3_errmsg(db);
      throw std::runtime_error("SQLite fatal error");
    }

    bindArguments<Types...>(db, statement, 1, args...);

    return SQLStatement(db, statement);
  }

 private:
  SQLStatement(sqlite3* db, sqlite3_stmt* stmt) : db_(db), stmt_(stmt, sqlite3_finalize) {}

  sqlite3* db_;
  std::unique_ptr<sqlite3_stmt, int (*)(sqlite3_stmt*)> stmt_;
};

#endif  // SQL_UTILS_H_
