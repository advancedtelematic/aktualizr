#include <gtest/gtest.h>

#include <string>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include "libuptiny/targets.h"
#include "libuptiny/common_data_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"


static void verify_targets_with_chunk_size(const std::string& targets_str, bool check_signatures, unsigned int chunk_size) {
  uptane_parse_targets_init();

  uptane_targets_t targets;
  memset(&targets, 0, sizeof(targets));

  std::string buf;
  uint16_t result=0;
  for(unsigned int i = 0; i < targets_str.length(); i += chunk_size) {
    buf += targets_str.substr(i, chunk_size); 

    int consumed = uptane_parse_targets_feed(buf.c_str(), buf.length(), &targets, &result);

    if (result == RESULT_ERROR) {
        LOG_ERROR << "uptane_parse_targets_feed returned error";
        ASSERT_TRUE(false);
    } else if (result == RESULT_WRONG_HW_ID) {
        LOG_ERROR << "Wrong hardware ID";
        ASSERT_TRUE(false);
    } else if (result == RESULT_VERSION_FAILED) {
        LOG_ERROR << "Wrong version";
        ASSERT_TRUE(false);
    } else if (result == RESULT_SIGNATURES_FAILED) {
        if (check_signatures) {
          LOG_ERROR << "Signature verification failed";
          ASSERT_TRUE(false);
        } else {
          break;
        }
    } else if (result == RESULT_END_FOUND) {
      if (!check_signatures) {
        LOG_ERROR << "Signature verification succeeded on invalid metadata";
        ASSERT_TRUE(false);
      }
    } else if(result & RESULT_END_NOT_FOUND) {
      if (!check_signatures) {
        LOG_ERROR << "Signature verification succeeded on invalid metadata";
        ASSERT_TRUE(false);
      } else {
        LOG_ERROR << "No target for test ECU";
        ASSERT_TRUE(false);
      }
    }
    if(consumed > 0) {
      buf = buf.substr(consumed);
    }
  }

  if(check_signatures) {
    EXPECT_EQ(result , RESULT_END_FOUND);
  } else {
    EXPECT_EQ(result , RESULT_SIGNATURES_FAILED);
  }

  EXPECT_EQ(targets.version, 2);
  EXPECT_EQ(std::string(targets.name), std::string("secondary_firmware.txt"));
  EXPECT_EQ(targets.hashes_num, 1);
  EXPECT_EQ(targets.hashes[0].alg, CRYPTO_HASH_SHA512);
  EXPECT_EQ(boost::algorithm::to_lower_copy(boost::algorithm::hex(std::string((const char*)targets.hashes[0].hash, 64))), "7dbae4c36a2494b731a9239911d3085d53d3e400886edb4ae2b9b78f40bda446649e83ba2d81653f614cc66f5dd5d4dbd95afba854f148afbfae48d0ff4cc38a");
  EXPECT_EQ(targets.length, 15);
  EXPECT_EQ(targets.expires.year, 3021);
  EXPECT_EQ(targets.expires.month, 7);
  EXPECT_EQ(targets.expires.day, 13);
  EXPECT_EQ(targets.expires.hour, 1);
  EXPECT_EQ(targets.expires.minute, 2);
  EXPECT_EQ(targets.expires.second, 3);

}

static void verify_targets(const std::string& targets_str, bool check_signatures) {
  verify_targets_with_chunk_size(targets_str, check_signatures, 1);
  verify_targets_with_chunk_size(targets_str, check_signatures, 3);
  verify_targets_with_chunk_size(targets_str, check_signatures, 7);
  verify_targets_with_chunk_size(targets_str, check_signatures, 10);
  verify_targets_with_chunk_size(targets_str, check_signatures, 20);
  verify_targets_with_chunk_size(targets_str, check_signatures, (unsigned int) targets_str.length());
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
