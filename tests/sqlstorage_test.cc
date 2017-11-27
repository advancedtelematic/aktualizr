#include "sqlstorage.h"

#include <string>

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include "logger.h"

TEST(sqlstorage, create_check) {
  Config config;

  SQLStorage storage(config);
  boost::filesystem::remove_all("tests/test_data/sql.db");
  Json::Value schema = Utils::parseJSONFile("config/storage/schema.0.json");
  EXPECT_TRUE(storage.createFromSchema("tests/test_data/sql.db", schema));
  EXPECT_TRUE(storage.checkSchema("tests/test_data/sql.db", schema));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerInit();
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif
