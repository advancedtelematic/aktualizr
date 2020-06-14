#include <gtest/gtest.h>
#include <libaktualizr/utils.h>
#include "storage/sql_utils.h"


TEST(sql_utils, PrepareRvalue) {
  TemporaryDirectory temp_dir;
  SQLite3Guard db((temp_dir.Path() / "test.db").c_str());

  db.exec("CREATE TABLE example(ex1 TEXT, ex2 TEXT);", NULL, NULL);
  // the arguments used in prepareStatement should last for the subsequent
  // sqlite calls (eg: `.step()`)
  std::string s2 = "test";
  auto statement =
      db.prepareStatement<std::string>("INSERT INTO example(ex1, ex2) VALUES (?,?);", temp_dir.PathString(), s2);
  EXPECT_EQ(statement.step(), SQLITE_DONE);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif
