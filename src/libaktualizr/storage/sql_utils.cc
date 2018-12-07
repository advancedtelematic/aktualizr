#include "sql_utils.h"
std::map<boost::filesystem::path, std::mutex> SQLite3Transaction::db_locks;
std::mutex SQLite3Transaction::db_locks_mutex;
