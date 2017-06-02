#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <string>
#include "boost/algorithm/hex.hpp"

#include "ostree.h"

TEST(ostree, constructor) {
  OstreePackage op("ecu_serial", "branch-name-hash", "description", "pull_uri");
  EXPECT_EQ(op.ecu_serial, "ecu_serial");
  EXPECT_EQ(op.ref_name, "branch-name-hash");
  EXPECT_EQ(op.branch_name, "branch-name");
  EXPECT_EQ(op.refhash, "hash");
  EXPECT_EQ(op.description, "description");
  EXPECT_EQ(op.pull_uri, "pull_uri");
}

TEST(ostree, fromjson) {
  Json::Value json;
  json["ecu_serial"] = "ecu_serial";
  json["ref_name"] = "branch-name-hash";
  json["description"] = "description";
  json["pull_uri"] = "pull_uri";
  OstreePackage op = OstreePackage::fromJson(json);
  EXPECT_EQ(op.ecu_serial, "ecu_serial");
  EXPECT_EQ(op.ref_name, "branch-name-hash");
  EXPECT_EQ(op.branch_name, "branch-name");
  EXPECT_EQ(op.refhash, "hash");
  EXPECT_EQ(op.description, "description");
  EXPECT_EQ(op.pull_uri, "pull_uri");
}

TEST(ostree, toEcuVersion) {
  OstreePackage op("ecu_serial", "branch-name-hash", "description", "pull_uri");
  Json::Value custom;
  custom["key"] = "value";
  Json::Value ecuver = op.toEcuVersion(custom);
  EXPECT_EQ(ecuver["custom"]["key"], "value");
  EXPECT_EQ(ecuver["ecu_serial"], "ecu_serial");
  EXPECT_EQ(ecuver["installed_image"]["fileinfo"]["hashes"]["sha256"], "hash");
  EXPECT_EQ(ecuver["installed_image"]["filepath"], "branch-name-hash");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
