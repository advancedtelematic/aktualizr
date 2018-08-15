#include <gtest/gtest.h>

#include "libuptiny/targets.h"
#include "libuptiny/firmware.h"
#include "libuptiny/state_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"

TEST(firmware, verify_nostate) {
  Json::Value targets_json = Utils::parseJSONFile("partial/tests/repo/repo/director/targets.json");
  std::string targets_str = Utils::jsonToCanonicalStr(targets_json);

  uint16_t result = 0x0000;
  uptane_targets_t targets;

  uptane_parse_targets_feed(targets_str.c_str(), targets_str.length(), &targets, &result);
  ASSERT_EQ(result, RESULT_END_FOUND);
  state_set_targets(&targets);

  std::string firmware = Utils::readFile("partial/tests/repo/repo/image/targets/secondary_firmware.txt");
  ASSERT_TRUE(uptane_verify_firmware_init());
  uptane_verify_firmware_feed(reinterpret_cast<const uint8_t*>(firmware.c_str()), firmware.length());
  EXPECT_TRUE(uptane_verify_firmware_finalize());
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
