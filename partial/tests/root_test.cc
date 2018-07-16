#include <gtest/gtest.h>
#include "libuptiny/root.h"

#include "logging/logging.h"
#include "utilities/utils.h"

static void check_root(const uptane_root_t& root) {
  EXPECT_EQ(root.expires.year, 3021);
  EXPECT_EQ(root.expires.month, 7);
  EXPECT_EQ(root.expires.day, 13);
  EXPECT_EQ(root.expires.hour, 1);
  EXPECT_EQ(root.expires.minute, 2);
  EXPECT_EQ(root.expires.second, 3);

  EXPECT_EQ(root.root_threshold, 1);
  EXPECT_EQ(root.root_keys_num, 1);
  EXPECT_TRUE(root.root_keys[0] != nullptr);
  EXPECT_EQ(root.root_keys[0]->key_type, CRYPTO_ALG_ED25519);
  EXPECT_EQ(root.targets_threshold, 1);
  EXPECT_EQ(root.targets_keys_num, 1);
  EXPECT_TRUE(root.targets_keys[0] != nullptr);
  EXPECT_EQ(root.targets_keys[0]->key_type, CRYPTO_ALG_ED25519);
}

TEST(tiny_root, parse_simple) {
  Json::Value root_json = Utils::parseJSONFile("partial/tests/repo/repo/director/1.root.json");
  std::string root_str = Utils::jsonToCanonicalStr(root_json);
  static uptane_root_t root;
  ASSERT_TRUE(uptane_parse_root(root_str.c_str(), root_str.length(), &root));

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
