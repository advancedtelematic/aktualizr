#include <boost/tokenizer.hpp>

#include <gtest/gtest.h>

#include "logging/logging.h"
#include "storage/sql_utils.h"
#include "storage/sqlstorage.h"
#include "uptane/directorrepository.h"
#include "uptane/imagesrepository.h"
#include "utilities/utils.h"

boost::filesystem::path test_data_dir;

typedef boost::tokenizer<boost::char_separator<char> > sql_tokenizer;

static std::map<std::string, std::string> parseSchema() {
  std::map<std::string, std::string> result;
  std::vector<std::string> tokens;
  enum { STATE_INIT, STATE_CREATE, STATE_INSERT, STATE_TABLE, STATE_NAME };
  boost::char_separator<char> sep(" \"\t\r\n", "(),;");
  std::string schema(libaktualizr_current_schema);
  sql_tokenizer tok(schema, sep);
  int parsing_state = STATE_INIT;

  std::string key;
  std::string value;
  for (sql_tokenizer::iterator it = tok.begin(); it != tok.end(); ++it) {
    std::string token = *it;
    if (value.empty()) {
      value = token;
    } else {
      value = value + " " + token;
    }
    switch (parsing_state) {
      case STATE_INIT:
        if (token == "CREATE") {
          parsing_state = STATE_CREATE;
        } else if (token == "INSERT") {
          parsing_state = STATE_INSERT;
        } else {
          return {};
        }
        break;
      case STATE_CREATE:
        if (token != "TABLE") {
          return {};
        }
        parsing_state = STATE_TABLE;
        break;
      case STATE_INSERT:
        // do not take these into account
        if (token == ";") {
          key.clear();
          value.clear();
          parsing_state = STATE_INIT;
        }
        break;
      case STATE_TABLE:
        if (token == "(" || token == ")" || token == "," || token == ";") {
          return {};
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
      default:
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
  std::map<std::string, std::string> tables = parseSchema();
  if (tables.empty()) {
    LOG_ERROR << "Could not parse schema";
    return false;
  }

  for (auto it = tables.begin(); it != tables.end(); ++it) {
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

struct TempSQLDb {
  std::unique_ptr<TemporaryDirectory> dir;
  boost::filesystem::path db_path;
};

static TempSQLDb makeDbWithVersion(DbVersion version) {
  TempSQLDb tdb;
  tdb.dir = std_::make_unique<TemporaryDirectory>();
  tdb.db_path = tdb.dir->Path() / "test.db";
  SQLite3Guard db(tdb.db_path.c_str());

  // manual migration runs

  for (uint32_t k = 0; k <= static_cast<uint32_t>(version); k++) {
    if (db.exec(libaktualizr_schema_migrations.at(k), nullptr, nullptr) != SQLITE_OK) {
      throw std::runtime_error("Migration run failed");
    }
  }

  return tdb;
}

/* Migrate forward through SQL schemas. */
TEST(sqlstorage, migrate) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();

  SQLStorage storage(config, false);
  boost::filesystem::remove_all(config.sqldb_path.get(config.path));
  EXPECT_FALSE(dbSchemaCheck(storage));
  EXPECT_TRUE(storage.dbMigrate());
  EXPECT_TRUE(dbSchemaCheck(storage));
}

/* Migrate backward through SQL schemas. */
TEST(sqlstorage, migrate_back) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();

  SQLStorage storage(config, false);

  SQLite3Guard db(temp_dir / "sql.db");
  auto ver = storage.getVersion();
  std::string migration_script =
      "\
  BEGIN TRANSACTION;\
    CREATE TABLE test_table(test_text TEXT NOT NULL, test_int INT NOT NULL);\
    INSERT INTO test_table VALUES(\"test_text\", 123);\
    DELETE FROM version;\
    INSERT INTO version VALUES( " +
      std::to_string(static_cast<int>(ver) + 1) +
      " );\
  COMMIT TRANSACTION;";
  db.exec(migration_script, NULL, NULL);

  std::string back_migration_script =
      "\
    DROP TABLE test_table; \
    DELETE FROM version;\
    INSERT INTO version VALUES(" +
      std::to_string(static_cast<int>(ver)) + ");";

  auto statement = db.prepareStatement("insert into rollback_migrations VALUES (?,?);", static_cast<int>(ver) + 1,
                                       back_migration_script);
  statement.step();

  EXPECT_EQ(static_cast<int>(storage.getVersion()), static_cast<int>(ver) + 1);
  EXPECT_TRUE(storage.dbMigrate());
  EXPECT_LT(static_cast<int>(storage.getVersion()), static_cast<int>(ver) + 1);
  EXPECT_TRUE(dbSchemaCheck(storage));
}

TEST(sqlstorage, rollback_to_15) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();

  SQLStorage storage(config, false);

  auto ver = storage.getVersion();
  ASSERT_TRUE(storage.dbMigrateBackward(static_cast<int>(ver), 15));
  ASSERT_EQ(static_cast<int>(storage.getVersion()), 15);
}

/* Automatically use latest SQL schema version when initializing database. */
TEST(sqlstorage, MigrationVersionCheck) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();
  SQLStorage storage(config, false);

  EXPECT_EQ(static_cast<int32_t>(storage.getVersion()), libaktualizr_schema_migrations.size() - 1);
}

/* Reject invalid SQL databases. */
TEST(sqlstorage, WrongDatabaseCheck) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();
  {
    SQLite3Guard db(config.sqldb_path.get(config.path).c_str());
    if (db.exec("CREATE TABLE some_table(somefield INTEGER);", NULL, NULL) != SQLITE_OK) {
      FAIL() << "Unable to create an SQL database for testing.";
    }
  }

  EXPECT_THROW(SQLStorage storage(config, false), StorageException);
}

TEST(sqlstorage, DbMigration7to8) {
  // it must use raw sql primitives because the SQLStorage object does automatic
  // migration + the api changes with time
  auto tdb = makeDbWithVersion(DbVersion(7));
  SQLite3Guard db(tdb.db_path.c_str());

  // test migration of `primary_keys` and `device_info`
  if (db.exec("INSERT INTO primary_keys VALUES ('priv', 'pub');", nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  if (db.exec("INSERT INTO device_info VALUES ('device', 1);", nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  // run migration
  if (db.exec(libaktualizr_schema_migrations.at(8), nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  // check values
  auto statement = db.prepareStatement("SELECT private, public FROM primary_keys;");
  if (statement.step() != SQLITE_ROW) {
    FAIL();
  }

  EXPECT_EQ(statement.get_result_col_str(0).value(), "priv");
  EXPECT_EQ(statement.get_result_col_str(1).value(), "pub");

  statement = db.prepareStatement("SELECT device_id, is_registered FROM device_info;");
  if (statement.step() != SQLITE_ROW) {
    FAIL();
  }

  EXPECT_EQ(statement.get_result_col_str(0).value(), "device");
  EXPECT_EQ(statement.get_result_col_int(1), 1);
}

TEST(sqlstorage, DbMigration12to13) {
  // it must use raw sql primitives because the SQLStorage object does automatic
  // migration + the api changes with time
  auto tdb = makeDbWithVersion(DbVersion(12));
  SQLite3Guard db(tdb.db_path.c_str());

  // test migration of installed_versions
  if (db.exec("INSERT INTO ecu_serials VALUES ('primary_ecu', 'primary_hw', 1);", nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  if (db.exec("INSERT INTO ecu_serials VALUES ('secondary_ecu', 'secondary_hw', 0);", nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  if (db.exec("INSERT INTO installed_versions VALUES ('sha256', 'v1', 1, 2);", nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  // run migration
  if (db.exec(libaktualizr_schema_migrations.at(13), nullptr, nullptr) != SQLITE_OK) {
    std::cout << db.errmsg() << "\n";
    FAIL() << "Migration 12 to 13 failed";
  }

  // check values
  auto statement = db.prepareStatement(
      "SELECT ecu_serial, sha256, name, hashes, length, is_current, is_pending FROM installed_versions;");
  if (statement.step() != SQLITE_ROW) {
    FAIL() << "installed_versions is empty";
  }

  EXPECT_EQ(statement.get_result_col_str(0).value(), "primary_ecu");
  EXPECT_EQ(statement.get_result_col_str(1).value(), "sha256");
  EXPECT_EQ(statement.get_result_col_str(2).value(), "v1");
  EXPECT_EQ(statement.get_result_col_str(3).value(), "");
  EXPECT_EQ(statement.get_result_col_int(4), 2);
  EXPECT_EQ(statement.get_result_col_int(5), 1);
  EXPECT_EQ(statement.get_result_col_int(6), 0);

  if (statement.step() != SQLITE_DONE) {
    FAIL() << "Too many rows";
  }
}

TEST(sqlstorage, DbMigration18to19) {
  // it must use raw sql primitives because the SQLStorage object does automatic
  // migration + the api changes with time
  auto tdb = makeDbWithVersion(DbVersion(18));
  SQLite3Guard db(tdb.db_path.c_str());

  // test migration of ecu_serials (wrong order is intended)
  if (db.exec("INSERT INTO ecu_serials VALUES ('secondary_ecu', 'secondary_hw', 0);", nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  if (db.exec("INSERT INTO ecu_serials VALUES ('primary_ecu', 'primary_hw', 1);", nullptr, nullptr) != SQLITE_OK) {
    FAIL();
  }

  // run migration
  if (db.exec(libaktualizr_schema_migrations.at(19), nullptr, nullptr) != SQLITE_OK) {
    std::cout << db.errmsg() << "\n";
    FAIL() << "Migration 18 to 19 failed";
  }

  // check values
  auto statement = db.prepareStatement("SELECT serial, hardware_id, is_primary FROM ecu_serials ORDER BY id;");
  if (statement.step() != SQLITE_ROW) {
    FAIL() << "ecu_serials is empty";
  }

  EXPECT_EQ(statement.get_result_col_str(0).value(), "primary_ecu");
  EXPECT_EQ(statement.get_result_col_str(1).value(), "primary_hw");
  EXPECT_EQ(statement.get_result_col_int(2), 1);

  if (statement.step() != SQLITE_ROW) {
    FAIL() << "ecu_serials contains only one element";
  }

  EXPECT_EQ(statement.get_result_col_str(0).value(), "secondary_ecu");
  EXPECT_EQ(statement.get_result_col_str(1).value(), "secondary_hw");
  EXPECT_EQ(statement.get_result_col_int(2), 0);

  if (statement.step() != SQLITE_DONE) {
    FAIL() << "Too many rows";
  }
}
/**
 * Check that old metadata is still valid
 */
TEST(sqlstorage, migrate_root_works) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path();

  boost::filesystem::remove_all(config.sqldb_path.get(config.path));
  boost::filesystem::copy(test_data_dir / "version5.sql", config.sqldb_path.get(config.path));
  SQLStorage storage(config, false);

  EXPECT_TRUE(storage.dbMigrate());
  EXPECT_TRUE(dbSchemaCheck(storage));

  // Director
  std::string raw_director_root;
  storage.loadRoot(&raw_director_root, Uptane::RepositoryType::Director(), Uptane::Version());
  Uptane::DirectorRepository director;
  EXPECT_TRUE(director.initRoot(raw_director_root));

  std::string raw_director_targets;
  storage.loadNonRoot(&raw_director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());

  EXPECT_TRUE(director.verifyTargets(raw_director_targets));

  // Images
  std::string raw_images_root;
  storage.loadRoot(&raw_images_root, Uptane::RepositoryType::Image(), Uptane::Version());
  Uptane::ImagesRepository imagesrepository;
  EXPECT_TRUE(imagesrepository.initRoot(raw_images_root));

  // Check that the roots are different and haven't been swapped
  EXPECT_NE(raw_director_root, raw_images_root);
  Json::Value director_json = Utils::parseJSON(raw_director_root);
  Json::Value sign = director_json["signed"];
  EXPECT_EQ(sign["_type"], "Root");
  EXPECT_TRUE(sign["keys"].isMember("1ba3b2932863c0c6e5ff857ecdeb476b69b8b9f9ba4e36723eb10faf7768818b"));
}

/* Migrate from the legacy filesystem storage. */
TEST(sqlstorage, migrate_from_fs) {
  TemporaryDirectory temp_dir;
  StorageConfig config;
  config.path = temp_dir.Path() / "config";

  // copy a snapshot from a real provisioned device using fs storage
  // (anonymized data)
  Utils::copyDir(test_data_dir / "fs_snapshot", config.path);
  ASSERT_GE(chmod(config.path.c_str(), S_IRWXU), 0);
  auto storage = INvStorage::newStorage(config);

  EXPECT_TRUE(storage->loadPrimaryKeys(nullptr, nullptr));
  EXPECT_TRUE(storage->loadTlsCreds(nullptr, nullptr, nullptr));
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director()));
  EXPECT_TRUE(storage->loadDeviceId(nullptr));

  EcuSerials serials;
  EXPECT_TRUE(storage->loadEcuSerials(&serials));
  EXPECT_EQ(serials.size(), 3);

  std::vector<MisconfiguredEcu> misconfigured;
  EXPECT_TRUE(storage->loadMisconfiguredEcus(&misconfigured));
  EXPECT_EQ(misconfigured.size(), 1);

  EXPECT_TRUE(storage->loadEcuRegistered());

  std::vector<Uptane::Target> installed;
  storage->loadPrimaryInstalledVersions(&installed, nullptr, nullptr);
  EXPECT_NE(installed.size(), 0);

  // note: installation result is not migrated anymore

  boost::filesystem::recursive_directory_iterator repo_dir_it(config.path), repo_dir_end;
  for (; repo_dir_it != repo_dir_end; ++repo_dir_it) {
    const auto p = repo_dir_it->path();
    if (p == config.sqldb_path.get(config.path)) {
      continue;
    }
    FAIL() << p << " should not exist anymore";
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  if (argc != 2) {
    std::cout << "Please pass the directory containing sql migration scripts as the first argument\n";
    return 1;
  }

  test_data_dir = argv[1];

  if (!boost::filesystem::is_directory(test_data_dir)) {
    std::cout << test_data_dir << " is not a directory\n";
    return 1;
  }

  return RUN_ALL_TESTS();
}
#endif
