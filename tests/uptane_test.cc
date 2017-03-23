#include <gtest/gtest.h>
#include <iostream>
#include <fstream>

#include <string>
#include "boost/algorithm/hex.hpp"

#include "uptane.h"
#include "crypto.h"

TEST(uptane, get_json) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  Uptane up(config);
  std::string expected = "D5912BC10D8AD16FEAF34715231FB4251CA72C7E3FEF4E34A48751D152006DFF";
  std::string result = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(up.getJSON(Uptane::Director, "root"))));
  EXPECT_EQ(expected, result);
  result = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(up.getJSON(Uptane::Repo, "root"))));
  EXPECT_EQ(expected, result);
}

TEST(uptane, get_endpoint) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  Uptane up(config);
  std::string result = up.getEndPointUrl(Uptane::Director, "root");
  EXPECT_EQ("https://director.com/root.json", result);
  
  result = up.getEndPointUrl(Uptane::Repo, "root");
  EXPECT_EQ("https://repo.com/device_id/root.json", result);
}

TEST(uptane, verify) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  Uptane up(config);
  Uptane::Verified verified;
  bool good = up.verify(Uptane::Director, "root", verified);
  EXPECT_EQ(good, true);
  EXPECT_EQ(verified.new_version, 1);
  EXPECT_EQ(verified.old_version, 1);
}



TEST(uptane, verify_data) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  Uptane up(config);
  Json::Value data_json = up.getJSON(Uptane::Director, "root");
  bool good = up.verifyData(Uptane::Director, "root", data_json);
  EXPECT_EQ(good, true);
}

TEST(uptane, verify_data_bad) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  Uptane up(config);
  Json::Value data_json = up.getJSON(Uptane::Director, "root");
  data_json.removeMember("signatures");
  bool good = up.verifyData(Uptane::Director, "root", data_json);
  EXPECT_EQ(good, false);
}


TEST(uptane, verify_data_unknow_type) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  Uptane up(config);
  Json::Value data_json = up.getJSON(Uptane::Director, "root");
  data_json["signatures"][0]["method"] = "badsignature";
  data_json["signatures"][1]["method"] = "badsignature";
  bool good = up.verifyData(Uptane::Director, "root", data_json);
  EXPECT_EQ(good, false);
}

TEST(uptane, verify_data_bed_keyid) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  Uptane up(config);
  Json::Value data_json = up.getJSON(Uptane::Director, "root");
  data_json["signatures"][0]["keyid"] = "badkeyid";
  data_json["signatures"][1]["keyid"] = "badsignature";
  bool good = up.verifyData(Uptane::Director, "root", data_json);
  EXPECT_EQ(good, false);
}


TEST(uptane, verify_data_bed_threshold) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  Uptane up(config);
  Json::Value data_json = up.getJSON(Uptane::Director, "root");
  data_json["signatures"][0]["keyid"] = "bedsignature";
  bool good = up.verifyData(Uptane::Director, "root", data_json);
  EXPECT_EQ(good, false);
}


#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
