#include <gtest/gtest.h>

#include <string>

#include "jsmn/jsmn.h"
#include "libuptiny/root_signed.h"
#include "libuptiny/common_data_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"

TEST(tiny_root_signed, parse_simple) {
  std::string signed_root_str = "{\"_type\":\"Root\",\"expires\":\"3019-02-10T13:33:02Z\",\"keys\":{\"5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43\":{\"keytype\":\"ED25519\",\"keyval\":{\"public\":\"EA4BE277FB02C42549929113156AD13D55D06F5E6F0DE2E775C50645D03A8F81\"}}},\"roles\":{\"root\":{\"keyids\":[\"5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43\"],\"threshold\":1},\"snapshot\":{\"keyids\":[\"5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43\"],\"threshold\":1},\"targets\":{\"keyids\":[\"5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43\"],\"threshold\":1},\"timestamp\":{\"keyids\":[\"5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43\"],\"threshold\":1}}}";

  jsmn_parser parser;
  jsmn_init(&parser);

  int parsed = jsmn_parse(&parser, signed_root_str.c_str(), signed_root_str.length(), token_pool, token_pool_size);

  EXPECT_GT(parsed, 0);
  unsigned int idx = 0;
  uptane_root_t root;
  EXPECT_TRUE(uptane_part_root_signed (signed_root_str.c_str(), &idx, &root));
  uptane_time_t time_in_str = {3019,2,10,13,33,2};
  EXPECT_EQ(memcmp(&root.expires, &time_in_str, sizeof(uptane_time_t)), 0); // C++ can't compare structs, shame on C++
  EXPECT_EQ(root.root_threshold, 1);
  EXPECT_EQ(root.root_keys_num, 1);
  EXPECT_TRUE(root.root_keys[0] != nullptr);
  EXPECT_EQ(root.root_keys[0]->key_type, ED25519);
  EXPECT_EQ(root.targets_threshold, 1);
  EXPECT_EQ(root.targets_keys_num, 1);
  EXPECT_TRUE(root.targets_keys[0] != nullptr);
  EXPECT_EQ(root.targets_keys[0]->key_type, ED25519);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
