#include "sqlstorage.h"

#include <gtest/gtest.h>

#include "logger.h"

const boost::filesystem::path sqlstorage_test_dir = "tests/test_sqlstorage";

TEST(sqlstorage, migrate0) {
  StorageConfig config;
  config.path = sqlstorage_test_dir;
  config.schema_version = 0;
  config.sqldb_path = "config/test.db";
  config.schemas_path = "config/storage";

  boost::filesystem::remove_all(config.sqldb_path);
  SQLStorage storage(config);
  EXPECT_FALSE(storage.dbCheck());
  EXPECT_TRUE(storage.dbMigrate());
  EXPECT_TRUE(storage.dbCheck());
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerInit();
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif
