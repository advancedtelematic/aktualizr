#include <gtest/gtest.h>

#include "sqlstorage.h"

#include "logger.h"

const boost::filesystem::path sqlstorage_test_dir = "tests/test_sqlstorage";

TEST(sqlstorage, migrate) {
  StorageConfig config;
  config.path = sqlstorage_test_dir;
  config.sqldb_path = "tests/test_sqlstorage/test.db";
  config.schemas_path = "config/storage";

  SQLStorage storage(config);
  boost::filesystem::remove_all(config.sqldb_path);

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
