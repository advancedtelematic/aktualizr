#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <string>
#include "boost/algorithm/hex.hpp"

#include "ostree.h"
#include "utils.h"
TEST(ostree, constructor) {
  OstreePackage op("ecu_serial", "branch-name-hash", "branch-name", "hash", "description", "pull_uri");
  EXPECT_EQ(op.ecu_serial, "ecu_serial");
  EXPECT_EQ(op.ref_name, "branch-name-hash");
  EXPECT_EQ(op.branch_name, "branch-name");
  EXPECT_EQ(op.refhash, "hash");
  EXPECT_EQ(op.description, "description");
  EXPECT_EQ(op.pull_uri, "pull_uri");
}

TEST(ostree, toEcuVersion) {
  OstreePackage op("ecu_serial", "branch-name-hash", "branch-name", "hash", "description", "pull_uri");
  Json::Value custom;
  custom["key"] = "value";
  Json::Value ecuver = op.toEcuVersion(custom);
  EXPECT_EQ(ecuver["custom"]["key"], "value");
  EXPECT_EQ(ecuver["ecu_serial"], "ecu_serial");
  EXPECT_EQ(ecuver["installed_image"]["fileinfo"]["hashes"]["sha256"], "hash");
  EXPECT_EQ(ecuver["installed_image"]["filepath"], "branch-name-hash");
}

TEST(ostree, parse_installed_packages) {
  Json::Value packages = Ostree::getInstalledPackages("tests/test_data/package.manifest");
  EXPECT_EQ(packages[0]["name"], "vim");
  EXPECT_EQ(packages[0]["version"], "1.0");
  EXPECT_EQ(packages[1]["name"], "emacs");
  EXPECT_EQ(packages[1]["version"], "2.0");
  EXPECT_EQ(packages[2]["name"], "bash");
  EXPECT_EQ(packages[2]["version"], "1.1");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
