#include <gtest/gtest.h>

#include <string>

#include "jsmn/jsmn.h"
#include "libuptiny/common_data_api.h"
#include "libuptiny/signatures.h"
#include "logging/logging.h"
#include "utilities/utils.h"

TEST(tiny_signatures, parse_simple) {
  Json::Value root_json = Utils::parseJSONFile("partial/tests/repo/repo/director/1.root.json");
  Json::Value signatures = root_json["signatures"];
  std::string signatures_str = Utils::jsonToStr(signatures);
  crypto_key_and_signature_t sigs[10];

  jsmn_parser parser;
  jsmn_init(&parser);
  int parsed = jsmn_parse(&parser, signatures_str.c_str(), signatures_str.length(), token_pool, token_pool_size);
  EXPECT_GT(parsed, 0);

  unsigned int token_idx = 0;
  EXPECT_EQ(uptane_parse_signatures(ROLE_ROOT, signatures_str.c_str(), &token_idx, sigs, 10, state_get_root()), 1);
  EXPECT_TRUE(sigs[0].key != nullptr);
  EXPECT_EQ(sigs[0].key->key_type, CRYPTO_ALG_ED25519);
  std::string sig_in_str = Utils::fromBase64("wfL5Ydvn83bKK1byanOYcTC9+4TK4EVnK+GadhgFJ3VxjO//jY/zIz4ChiiR9DBU59ZUiTF3IICvZVFUt4DTAg==");
  std::string sig_parsed = std::string(reinterpret_cast<char*>(sigs[0].sig), CRYPTO_MAX_SIGNATURE_LEN);
  EXPECT_EQ(sig_in_str, sig_parsed);
}

TEST(tiny_signatures, parse_one_of_two) {
  Json::Value root_json = Utils::parseJSONFile("partial/tests/repo/repo/director/1.root.json");
  Json::Value signatures = root_json["signatures"];
  Json::Value second_signature;
  second_signature["keyid"] = "5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43";
  second_signature["method"] = "ed25519";
  second_signature["sig"] = "9hU1Bj4mrONkvv8iQHfZnd4obUFVkHHLwPjqoRRquSilZAJMFbAnO/Bx0PBUNfjcdV7xfytw/uNcBdv6lUmrAw==";
  signatures.append(second_signature);
  std::string signatures_str = Utils::jsonToStr(signatures);
  crypto_key_and_signature_t sigs[10];

  jsmn_parser parser;
  jsmn_init(&parser);
  int parsed = jsmn_parse(&parser, signatures_str.c_str(), signatures_str.length(), token_pool, token_pool_size);
  EXPECT_GT(parsed, 0);

  unsigned int token_idx = 0;
  EXPECT_EQ(uptane_parse_signatures(ROLE_ROOT, signatures_str.c_str(), &token_idx, sigs, 10, state_get_root()), 1);
  EXPECT_TRUE(sigs[0].key != nullptr);
  EXPECT_TRUE(sigs[0].key->key_type == CRYPTO_ALG_ED25519);
  std::string sig_in_str = Utils::fromBase64("wfL5Ydvn83bKK1byanOYcTC9+4TK4EVnK+GadhgFJ3VxjO//jY/zIz4ChiiR9DBU59ZUiTF3IICvZVFUt4DTAg==");
  std::string sig_parsed = std::string(reinterpret_cast<char*>(sigs[0].sig), CRYPTO_MAX_SIGNATURE_LEN);
  EXPECT_EQ(sig_in_str, sig_parsed);
}

TEST(tiny_signatures, parse_plus_garbage) {
  Json::Value root_json = Utils::parseJSONFile("partial/tests/repo/repo/director/1.root.json");
  Json::Value signatures = root_json["signatures"];
  Json::Value second_signature;
  second_signature["keyid"] = "5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43";
  second_signature["method"] = "ed25519";
  second_signature["sig"] = "9hU1Bj4mrONkvv8iQHfZnd4obUFVkHHLwPjqoRRquSilZAJMFbAnO/Bx0PBUNfjcdV7xfytw/uNcBdv6lUmrAw==";
  signatures.append(second_signature);
  signatures[0]["newfield"]["subfield1"] = "value1";
  signatures[0]["newfield"]["subfield2"][0] = "value2";
  signatures[0]["newfield"]["subfield2"][1] = "value3";
  signatures[1]["newfield"]["subfield1"] = "value4";
  signatures[1]["newfield"]["subfield2"][0] = "value5";
  std::string signatures_str = Utils::jsonToStr(signatures);
 
  crypto_key_and_signature_t sigs[10];

  jsmn_parser parser;
  jsmn_init(&parser);
  int parsed = jsmn_parse(&parser, signatures_str.c_str(), signatures_str.length(), token_pool, token_pool_size);
  EXPECT_GT(parsed, 0);

  unsigned int token_idx = 0;
  EXPECT_EQ(uptane_parse_signatures(ROLE_ROOT, signatures_str.c_str(), &token_idx, sigs, 10, state_get_root()), 1);
  EXPECT_TRUE(sigs[0].key != nullptr);
  EXPECT_TRUE(sigs[0].key->key_type == CRYPTO_ALG_ED25519);
  std::string sig_in_str = Utils::fromBase64("wfL5Ydvn83bKK1byanOYcTC9+4TK4EVnK+GadhgFJ3VxjO//jY/zIz4ChiiR9DBU59ZUiTF3IICvZVFUt4DTAg==");
  std::string sig_parsed = std::string(reinterpret_cast<char*>(sigs[0].sig), CRYPTO_MAX_SIGNATURE_LEN);
  EXPECT_EQ(sig_in_str, sig_parsed);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
