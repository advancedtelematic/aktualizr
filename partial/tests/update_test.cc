#include <gtest/gtest.h>

#include "libuptiny/targets.h"
#include "libuptiny/firmware.h"
#include "libuptiny/manifest.h"
#include "libuptiny/state_api.h"
#include "logging/logging.h"
#include "utilities/utils.h"
#include "crypto/crypto.h"

TEST(update, full) {
  Json::Value targets_json = Utils::parseJSONFile("partial/tests/repo/repo/director/targets.json");
  std::string targets_str = Utils::jsonToCanonicalStr(targets_json);

  uint16_t result = 0x0000;
  uptane_targets_t targets;

  uptane_parse_targets_feed(targets_str.c_str(), (int16_t) targets_str.length(), &targets, &result);
  ASSERT_EQ(result, RESULT_END_FOUND);
  state_set_targets(&targets);

  std::string firmware = Utils::readFile("partial/tests/repo/repo/image/targets/secondary_firmware.txt");
  ASSERT_TRUE(uptane_verify_firmware_init());
  uptane_verify_firmware_feed(reinterpret_cast<const uint8_t*>(firmware.c_str()), firmware.length());
  ASSERT_TRUE(uptane_verify_firmware_finalize());

  uptane_firmware_confirm();

  char signatures_buf[1000];
  char signed_buf[1000];

  uptane_write_manifest(signed_buf, signatures_buf);
  Json::Value manifest_json;
  manifest_json["signatures"] = Utils::parseJSON(std::string(signatures_buf));
  manifest_json["signed"] = Utils::parseJSON(std::string(signed_buf));

  const crypto_key_t* public_key;
  const uint8_t* private_key;
  state_get_device_key(&public_key, &private_key);

  PublicKey cpp_public_key{boost::algorithm::hex(std::string((const char*)public_key->keyval, 32)), KeyType::kED25519};
  EXPECT_TRUE(cpp_public_key.VerifySignature(manifest_json["signatures"][0]["sig"].asString(), Utils::jsonToCanonicalStr(manifest_json["signed"])));

  EXPECT_EQ(manifest_json["signed"]["installed_image"]["filepath"].asString(), "secondary_firmware.txt");
  EXPECT_EQ(manifest_json["signed"]["installed_image"]["fileinfo"]["length"].asInt(), 15);
  EXPECT_EQ(manifest_json["signed"]["installed_image"]["fileinfo"]["hashes"]["sha512"].asString(), "7dbae4c36a2494b731a9239911d3085d53d3e400886edb4ae2b9b78f40bda446649e83ba2d81653f614cc66f5dd5d4dbd95afba854f148afbfae48d0ff4cc38a");
}
#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
