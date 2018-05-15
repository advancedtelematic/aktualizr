#include <boost/tokenizer.hpp>

#include <gtest/gtest.h>

#include "logging/logging.h"
#include "storage/sql_utils.h"
#include "storage/sqlstorage.h"
#include "utilities/utils.h"

boost::filesystem::path schemas_path;

typedef boost::tokenizer<boost::char_separator<char> > sql_tokenizer;

static std::map<std::string, std::string> parseSchema(int version) {
  boost::filesystem::path schema_file = schemas_path / (std::string("schema.") + std::to_string(version) + ".sql");
  std::string schema = Utils::readFile(schema_file.string());
  std::map<std::string, std::string> result;
  std::vector<std::string> tokens;

  enum { STATE_INIT, STATE_CREATE, STATE_TABLE, STATE_NAME };
  boost::char_separator<char> sep(" \"\t\r\n", "(),;");
  sql_tokenizer tok(schema, sep);
  int parsing_state = STATE_INIT;

  std::string key;
  std::string value;
  for (sql_tokenizer::iterator it = tok.begin(); it != tok.end(); ++it) {
    std::string token = *it;
    if (value.empty())
      value = token;
    else
      value = value + " " + token;
    switch (parsing_state) {
      case STATE_INIT:
        if (token != "CREATE") {
          return std::map<std::string, std::string>();
        }
        parsing_state = STATE_CREATE;
        break;
      case STATE_CREATE:
        if (token != "TABLE") {
          return std::map<std::string, std::string>();
        }
        parsing_state = STATE_TABLE;
        break;
      case STATE_TABLE:
        if (token == "(" || token == ")" || token == "," || token == ";") {
          return std::map<std::string, std::string>();
        }
        key = token;
        parsing_state = STATE_NAME;
        break;
      case STATE_NAME:
        if (token == ";") {
          result[key] = value;
          key.clear();
          value.clear();
          parsing_state = STATE_INIT;
        }
        break;
    }
  }
  return result;
}

static bool tableSchemasEqual(const std::string& left, const std::string& right) {
  boost::char_separator<char> sep(" \"\t\r\n", "(),;");
  sql_tokenizer tokl(left, sep);
  sql_tokenizer tokr(right, sep);

  sql_tokenizer::iterator it_l;
  sql_tokenizer::iterator it_r;
  for (it_l = tokl.begin(), it_r = tokr.begin(); it_l != tokl.end() && it_r != tokr.end(); ++it_l, ++it_r) {
    if (*it_l != *it_r) return false;
  }
  return (it_l == tokl.end()) && (it_r == tokr.end());
}

static bool dbSchemaCheck(SQLStorage& storage) {
  std::map<std::string, std::string> tables = parseSchema(kSqlSchemaVersion);

  for (std::map<std::string, std::string>::iterator it = tables.begin(); it != tables.end(); ++it) {
    std::string schema_from_db = storage.getTableSchemaFromDb(it->first);
    if (!tableSchemasEqual(schema_from_db, it->second)) {
      LOG_ERROR << "Schemas don't match for " << it->first;
      LOG_ERROR << "Expected " << it->second;
      LOG_ERROR << "Found " << schema_from_db;
      return false;
    }
  }
  return true;
}

TEST(sqlstorage, migrate) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();
  config.sqldb_path = temp_dir.Path() / "test.db";
  config.schemas_path = "config/schemas";

  SQLStorage storage(config);
  boost::filesystem::remove_all(config.sqldb_path);

  EXPECT_FALSE(dbSchemaCheck(storage));
  EXPECT_TRUE(storage.dbMigrate());
  EXPECT_TRUE(dbSchemaCheck(storage));
}

TEST(sqlstorage, MissingSchema) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();
  config.sqldb_path = temp_dir.Path() / "test.db";
  config.schemas_path = "config/schemas-missing";

  EXPECT_THROW(SQLStorage storage(config), std::runtime_error);
}

TEST(sqlstorage, MigrationVersionCheck) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();
  config.sqldb_path = temp_dir.Path() / "test.db";
  config.schemas_path = "config/schemas";
  SQLStorage storage(config);

  EXPECT_EQ(storage.getVersion(), kSqlSchemaVersion);
}

TEST(sqlstorage, WrongDatabaseCheck) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();
  config.sqldb_path = temp_dir.Path() / "test.db";
  config.schemas_path = "config/schemas";
  SQLite3Guard db(config.sqldb_path.c_str());
  if (db.exec("CREATE TABLE some_table(somefield INTEGER);", NULL, NULL) != SQLITE_OK) {
    FAIL();
  }

  SQLStorage storage(config);
  EXPECT_EQ(storage.getVersion(), -2);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the schemas directory as an input argument.\n";
    return EXIT_FAILURE;
  }
  schemas_path = argv[1];
  return RUN_ALL_TESTS();
}
#endif
