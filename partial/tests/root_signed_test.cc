#include <gtest/gtest.h>

#include <string>

#include "jsmn/jsmn.h"
#include "libuptiny/root_signed.h"
#include "libuptiny/common_data_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"

static void check_root(const uptane_root_t& root) {
  EXPECT_EQ(root.expires.year, 3021);
  EXPECT_EQ(root.expires.month, 7);
  EXPECT_EQ(root.expires.day, 13);
  EXPECT_EQ(root.expires.hour, 1);
  EXPECT_EQ(root.expires.minute, 2);

  EXPECT_EQ(root.root_threshold, 1);
  EXPECT_EQ(root.root_keys_num, 1);
  EXPECT_TRUE(root.root_keys[0] != nullptr);
  EXPECT_EQ(root.root_keys[0]->key_type, CRYPTO_ALG_ED25519);
  EXPECT_EQ(root.targets_threshold, 1);
  EXPECT_EQ(root.targets_keys_num, 1);
  EXPECT_TRUE(root.targets_keys[0] != nullptr);
  EXPECT_EQ(root.targets_keys[0]->key_type, CRYPTO_ALG_ED25519);
}

TEST(tiny_root_signed, parse_simple) {
  Json::Value root_json = Utils::parseJSONFile("partial/tests/repo/repo/director/1.root.json");
  Json::Value signed_root = root_json["signed"];
  std::string signed_root_str = Utils::jsonToStr(signed_root);

  jsmn_parser parser;
  jsmn_init(&parser);

  int parsed = jsmn_parse(&parser, signed_root_str.c_str(), signed_root_str.length(), token_pool, token_pool_size);

  EXPECT_GT(parsed, 0);
  unsigned int idx = 0;
  uptane_root_t root;
  EXPECT_TRUE(uptane_part_root_signed (signed_root_str.c_str(), &idx, &root));
  check_root(root);
}

TEST(tiny_root_signed, parse_with_garbage) {
  Json::Value root_json = Utils::parseJSONFile("partial/tests/repo/repo/director/1.root.json");
  Json::Value signed_root = root_json["signed"];
  signed_root["newtopfield"]["key"] = "value";
  signed_root["anothernewtopfield"] = "anothervalue";
  signed_root["keys"]["ce69f17a69ca6e000d676010330b3a23c2c97ee45fe9e1ec97786a8d7bde967f"]["keysmell"] = "fruity";
  signed_root["keys"]["ce69f17a69ca6e000d676010330b3a23c2c97ee45fe9e1ec97786a8d7bde967f"]["keyval"]["private"] = "wonttell";
  signed_root["roles"]["root"]["yetanothernewfield"][0] = "value1";
  signed_root["roles"]["root"]["yetanothernewfield"][1] = "value2";
  std::string signed_root_str = Utils::jsonToStr(signed_root);

  jsmn_parser parser;
  jsmn_init(&parser);

  int parsed = jsmn_parse(&parser, signed_root_str.c_str(), signed_root_str.length(), token_pool, token_pool_size);

  EXPECT_GT(parsed, 0);
  unsigned int idx = 0;
  uptane_root_t root;
  EXPECT_TRUE(uptane_part_root_signed (signed_root_str.c_str(), &idx, &root));
  check_root(root);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
