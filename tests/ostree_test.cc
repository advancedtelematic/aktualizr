#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <string>
#include "boost/algorithm/hex.hpp"

#include "ostree.h"

TEST(ostree, constructor) {
  OstreePackage op("ecu_serial", "ref_name", "commit", "description", "pull_uri");
  EXPECT_EQ(op.ecu_serial, "ecu_serial");
  EXPECT_EQ(op.ref_name, "ref_name");
  EXPECT_EQ(op.commit, "commit");
  EXPECT_EQ(op.description, "description");
  EXPECT_EQ(op.pull_uri, "pull_uri");
}

TEST(ostree, fromjson) {
  Json::Value json;
  json["ecu_serial"] = "ecu_serial";
  json["ref_name"] = "ref_name";
  json["commit"] = "commit";
  json["description"] = "description";
  json["pull_uri"] = "pull_uri";
  OstreePackage op = OstreePackage::fromJson(json);
  EXPECT_EQ(op.ecu_serial, "ecu_serial");
  EXPECT_EQ(op.ref_name, "ref_name");
  EXPECT_EQ(op.commit, "commit");
  EXPECT_EQ(op.description, "description");
  EXPECT_EQ(op.pull_uri, "pull_uri");
}

TEST(ostree, toEcuVersion) {
  OstreePackage op("ecu_serial", "ref_name", "commit", "description", "pull_uri");
  Json::Value custom;
  custom["key"] = "value";
  Json::Value ecuver = op.toEcuVersion(custom);
  EXPECT_EQ(ecuver["custom"]["key"], "value");
  EXPECT_EQ(ecuver["ecu_serial"], "ecu_serial");
  EXPECT_EQ(ecuver["installed_image"]["fileinfo"]["hashes"]["sha256"], "commit");
  EXPECT_EQ(ecuver["installed_image"]["filepath"], "ref_name");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
