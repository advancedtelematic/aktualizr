#include <gtest/gtest.h>

#include <string>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include "libuptiny/targets.h"
#include "libuptiny/common_data_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"

void verify_targets(const std::string& targets_str, bool check_signatures) {
  uptane_parse_targets_init();

  uptane_targets_t targets;

  std::string buf;
  int in_signed = false;
  int was_signed = false;
  for(int i = 0; i < targets_str.length(); i += 10) {
    int buf_rest = buf.length();
    buf += targets_str.substr(i, 10); 

    uint16_t result;
    int consumed = uptane_parse_targets_feed(buf.c_str(), buf.length(), &targets, &result);

    if (result == RESULT_ERROR) {
        LOG_ERROR << "uptane_parse_targets_feed returned error";
        ASSERT_TRUE(false);
        return;
    } else if (result == RESULT_WRONG_HW_ID) {
        LOG_ERROR << "Wrong hardware ID";
        ASSERT_TRUE(false);
        return;
    }

    if(result & RESULT_BEGIN_SIGNED) {
      int begin_signed = uptane_targets_get_begin_signed();
      LOG_ERROR << "BEGIN_SIGNED AT " << begin_signed << " IN " << buf; 
      in_signed = true;
      int num_signatures = uptane_targets_get_num_signatures();
      for (int j = 0; j < num_signatures; j++) {
        crypto_verify_init(&crypto_ctx_pool[j], &signature_pool[j]);
        LOG_ERROR << "CRYPTO FEED " << buf.substr(begin_signed); 
        crypto_verify_feed(&crypto_ctx_pool[j], reinterpret_cast<const uint8_t*>(buf.substr(begin_signed).c_str()), buf.substr(begin_signed).length()); 
      }
    }

    if(result & RESULT_END_SIGNED) {
      ASSERT_TRUE(in_signed);
      in_signed = false;
      int end_signed = uptane_targets_get_end_signed();
      LOG_ERROR << "END_SIGNED AT " << end_signed << " IN " << buf; 
      int num_signatures = uptane_targets_get_num_signatures();
      int num_valid_signatures = 0;
      for (int j = 0; j < num_signatures; j++) {
        LOG_ERROR << "CRYPTO FEED " << std::string(targets_str.c_str()+i, end_signed-buf_rest); 
        crypto_verify_feed(&crypto_ctx_pool[j], reinterpret_cast<const uint8_t*>(targets_str.c_str())+i, end_signed-buf_rest); 
        if (crypto_verify_result(&crypto_ctx_pool[j])) {
          ++num_valid_signatures;
        }
      }
      if(check_signatures) {
        ASSERT_GE (num_valid_signatures, state_get_root()->targets_threshold);
      } else {
        ASSERT_EQ(num_valid_signatures, 0);
      }
      was_signed = true;
    }

    if(result & RESULT_END_FOUND) {
      ASSERT_TRUE(was_signed);
      EXPECT_TRUE(i + 10 >= targets_str.length());
      EXPECT_EQ(targets.version, 2);
      EXPECT_EQ(std::string(targets.name), std::string("secondary_firmware.txt"));
      EXPECT_EQ(targets.hashes_num, 1);
      EXPECT_EQ(targets.hashes[0].alg, CRYPTO_HASH_SHA512);
      EXPECT_EQ(boost::algorithm::to_lower_copy(boost::algorithm::hex(std::string((const char*)targets.hashes[0].hash, 64))), "7dbae4c36a2494b731a9239911d3085d53d3e400886edb4ae2b9b78f40bda446649e83ba2d81653f614cc66f5dd5d4dbd95afba854f148afbfae48d0ff4cc38a");
      EXPECT_EQ(targets.length, 15);
      uptane_time_t time_in_str = {3021,7,13,1,2,3};
      EXPECT_EQ(memcmp(&targets.expires, &time_in_str, sizeof(uptane_time_t)), 0);
      return;
    }
   
    if(result & RESULT_END_NOT_FOUND) {
      ASSERT_FALSE(in_signed);
      LOG_ERROR << "No target for test ECU";
      ASSERT_TRUE(false);
    }

    buf = buf.substr(consumed);

    if (in_signed && !(result & (RESULT_BEGIN_SIGNED | RESULT_END_SIGNED))) {
      int num_signatures = uptane_targets_get_num_signatures();
      for (int j = 0; j < num_signatures; j++) {
        LOG_ERROR << "CRYPTO FEED " << std::string(targets_str.c_str()+i, 10); 
        crypto_verify_feed(&crypto_ctx_pool[j], reinterpret_cast<const uint8_t*>(targets_str.c_str())+i, 10); 
      }
    }
  }
}

TEST(tiny_targets, parse_simple) {
  Json::Value targets_json = Utils::parseJSONFile("partial/tests/repo/repo/director/targets.json");
  std::string targets_str = Utils::jsonToCanonicalStr(targets_json);

  verify_targets(targets_str, true);
}

TEST(tiny_targets, parse_with_garbage) {
  Json::Value targets_json = Utils::parseJSONFile("partial/tests/repo/repo/director/targets.json");
  targets_json["newtopfield"]["key"] = "value";
  targets_json["signed"]["newsignedfield"]["key"] = "value";
  targets_json["signed"]["targets"]["secondary_firmware.txt"]["version"] = "1";
  targets_json["signed"]["targets"]["secondary_firmware.txt"]["custom"]["morecustom"]["key"] = "value";
  targets_json["signed"]["targets"]["secondary_firmware.txt"]["custom"]["ecuIdentifiers"]["uptane_secondary_1"]["key"] = "value";
  targets_json["signed"]["targets"]["secondary_firmware.txt"]["custom"]["ecuIdentifiers"]["uptane_secondary_2"]["hardwareId"] = "another_test_uptane_secondary";
  std::string targets_str = Utils::jsonToCanonicalStr(targets_json);

  LOG_ERROR << "PARSING " << targets_str;

  verify_targets(targets_str, false);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
