#include <gtest/gtest.h>

#include <string>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include "libuptiny/targets.h"
#include "libuptiny/common_data_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"

TEST(tiny_targets, parse_simple) {
  std::string targets_str = "{\"signatures\":[{\"keyid\":\"a70a72561409b9e0bc67b7625865fed801a57771102514b6de5f3b85f1bf27c2\",\"method\":\"ed25519\",\"sig\":\"zgz8Yy6+OytNj4GIySEhhYpK/l/ZmTfrm6vBfJ50BJWkKq/W1HVApPPk4AWpL8jOjZq5IAPFPKLzNwqmXOx8CQ==\"}],\"signed\":{\"_type\":\"Targets\",\"expires\":\"3021-07-13T01:02:03Z\",\"targets\":{\"secondary_firmware.txt\":{\"custom\":{\"ecuIdentifiers\":{\"uptane_secondary_1\":{\"hardwareId\":\"test_uptane_secondary\"}}},\"hashes\":{\"sha256\":\"1bbb15aa921ffffd5079567d630f43298dbe5e7cbc1b14e0ccdd6718fde28e47\",\"sha512\":\"7dbae4c36a2494b731a9239911d3085d53d3e400886edb4ae2b9b78f40bda446649e83ba2d81653f614cc66f5dd5d4dbd95afba854f148afbfae48d0ff4cc38a\"},\"length\":15}},\"version\":2}}";

  uptane_parse_targets_init();

  uptane_targets_t targets;

  std::string buf;
  int in_signed = false;
  for(int i = 0; i < targets_str.length(); i += 10) {
    int buf_rest = buf.length();
    buf += targets_str.substr(i, 10); 

    targets_result_t result;
    int consumed = uptane_parse_targets_feed(buf.c_str(), buf.length(), &targets, &result);

    switch (result) {
      case RESULT_ERROR:
        LOG_ERROR << "uptane_parse_targets_feed returned error";
        ASSERT_TRUE(false);
        return;
      case RESULT_WRONG_HW_ID:
        LOG_ERROR << "Wrong hardware ID";
        ASSERT_TRUE(false);
        return;
      case RESULT_BEGIN_SIGNED:
        {
          int begin_signed = uptane_targets_get_begin_signed();
          LOG_ERROR << "BEGIN_SIGNED AT " << begin_signed << " IN " << buf; 
          in_signed = true;
          int num_signatures = uptane_targets_get_num_signatures();
          for (int j = 0; j < num_signatures; j++) {
            crypto_verify_init(&crypto_ctx_pool[j], &signature_pool[j]);
            LOG_ERROR << "CRYPTO FEED " << buf.substr(begin_signed); 
            crypto_verify_feed(&crypto_ctx_pool[j], reinterpret_cast<const uint8_t*>(buf.substr(begin_signed).c_str()), buf.substr(begin_signed).length()); 
          }
          buf = buf.substr(consumed);
        }
        break;
      case RESULT_END_SIGNED:
        {
          ASSERT_TRUE(in_signed);
          int end_signed = uptane_targets_get_end_signed();
          LOG_ERROR << "END_SIGNED AT " << end_signed << " IN " << buf; 
          int num_signatures = uptane_targets_get_num_signatures();
          for (int j = 0; j < num_signatures; j++) {
            LOG_ERROR << "CRYPTO FEED " << std::string(targets_str.c_str()+i, end_signed-buf_rest); 
            crypto_verify_feed(&crypto_ctx_pool[j], reinterpret_cast<const uint8_t*>(targets_str.c_str())+i, end_signed-buf_rest); 
            EXPECT_TRUE(crypto_verify_result(&crypto_ctx_pool[j]));
          }
          buf = buf.substr(consumed);
        }
        break;
      case RESULT_BEGIN_AND_END_SIGNED:
        // TODO
        buf = buf.substr(consumed);
        break;
      case RESULT_IN_PROGRESS:
        buf = buf.substr(consumed);

        if (in_signed) {
          int num_signatures = uptane_targets_get_num_signatures();
          for (int j = 0; j < num_signatures; j++) {
            LOG_ERROR << "CRYPTO FEED " << std::string(targets_str.c_str()+i, 10); 
            crypto_verify_feed(&crypto_ctx_pool[j], reinterpret_cast<const uint8_t*>(targets_str.c_str())+i, 10); 
          }
        }
        break;
      case RESULT_FOUND:
        {
          EXPECT_TRUE(i + 10 >= targets_str.length());
          EXPECT_EQ(targets.version, 2);
          EXPECT_EQ(std::string(targets.name), std::string("secondary_firmware.txt"));
          EXPECT_EQ(targets.hashes_num, 1);
          EXPECT_EQ(targets.hashes[0].alg, SHA512);
          EXPECT_EQ(boost::algorithm::to_lower_copy(boost::algorithm::hex(std::string((const char*)targets.hashes[0].hash, 64))), "7dbae4c36a2494b731a9239911d3085d53d3e400886edb4ae2b9b78f40bda446649e83ba2d81653f614cc66f5dd5d4dbd95afba854f148afbfae48d0ff4cc38a");
          EXPECT_EQ(targets.length, 15);
          uptane_time_t time_in_str = {3021,7,13,1,2,3};
          EXPECT_EQ(memcmp(&targets.expires, &time_in_str, sizeof(uptane_time_t)), 0);
        }
   
        return;
      case RESULT_NOT_FOUND:
        LOG_ERROR << "No target for test ECU";
        ASSERT_TRUE(false);
        return;
      default:
        LOG_ERROR << "Unexpected return value from uptane_parse_targets_feed: " << result;
        ASSERT_TRUE(false);
        return;
    }
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
