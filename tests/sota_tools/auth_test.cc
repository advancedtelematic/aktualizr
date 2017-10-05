#include <gtest/gtest.h>

#include <string>

#include "authenticate.h"
#include "treehub_server.h"

TEST(authenticate, good_zip) {
  std::string filepath = "sota_tools/auth_test_good.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(0, r);
}

TEST(authenticate, bad_zip) {
  std::string filepath = "sota_tools/auth_test_bad.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, no_json_zip) {
  std::string filepath = "sota_tools/auth_test_no_json.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, good_json) {
  std::string filepath = "sota_tools/auth_test_good.json";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(0, r);
}

TEST(authenticate, bad_json) {
  std::string filepath = "sota_tools/auth_test_bad.json";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, invalid_file) {
  std::string filepath = "sota_tools/auth_test.cc";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
