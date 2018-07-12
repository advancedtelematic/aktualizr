#include <gtest/gtest.h>

#include <string>

#include "jsmn/jsmn.h"
#include "libuptiny/root_signed.h"
#include "libuptiny/common_data_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"

TEST(tiny_root_signed, parse_simple) {
 std::string signed_root_str = "{\"_type\":\"Root\",\"expires\":\"3021-07-13T01:02:03Z\",\"keys\":{\"a70a72561409b9e0bc67b7625865fed801a57771102514b6de5f3b85f1bf27c2\":{\"keytype\":\"ED25519\",\"keyval\":{\"public\":\"C18143A73BD7EC00C0ACC194CAD973118BDE920BF4C0629F7F95209D42283CB6\"}}},\"roles\":{\"root\":{\"keyids\":[\"a70a72561409b9e0bc67b7625865fed801a57771102514b6de5f3b85f1bf27c2\"],\"threshold\":1},\"snapshot\":{\"keyids\":[\"a70a72561409b9e0bc67b7625865fed801a57771102514b6de5f3b85f1bf27c2\"],\"threshold\":1},\"targets\":{\"keyids\":[\"a70a72561409b9e0bc67b7625865fed801a57771102514b6de5f3b85f1bf27c2\"],\"threshold\":1},\"timestamp\":{\"keyids\":[\"a70a72561409b9e0bc67b7625865fed801a57771102514b6de5f3b85f1bf27c2\"],\"threshold\":1}},\"version\":1}";

  jsmn_parser parser;
  jsmn_init(&parser);

  int parsed = jsmn_parse(&parser, signed_root_str.c_str(), signed_root_str.length(), token_pool, token_pool_size);

  EXPECT_GT(parsed, 0);
  unsigned int idx = 0;
  uptane_root_t root;
  EXPECT_TRUE(uptane_part_root_signed (signed_root_str.c_str(), &idx, &root));
  uptane_time_t time_in_str = {3021,7,13,1,2,3};
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
