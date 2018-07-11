#include <gtest/gtest.h>

#include <string>

#include "libuptiny/signatures.h"
#include "logging/logging.h"
#include "utilities/utils.h"

TEST(tiny_signatures, parse_simple) {
  std::string signatures_str = "[{\"keyid\":\"982daae1f8bc4c81e259112c0baaf3ca49a20bac172044ec735868ec98c3f406\",\"method\":\"ed25519\",\"sig\":\"zrzJqjJS1RhikRZohH5/m0x1DeK2na+O7u6Zhx8o7kctruiayGyevnDuA45zPIUR5tQAZ85a1BwDX6BaazgXCw==\"}]";
  crypto_key_and_signature_t sigs[10];

  EXPECT_EQ(uptane_parse_signatures(ROLE_ROOT, signatures_str.c_str(), signatures_str.length(), sigs, 10), 1);
  EXPECT_TRUE(sigs[0].key != nullptr);
  EXPECT_EQ(sigs[0].key->key_type, ED25519);
  std::string sig_in_str = Utils::fromBase64("zrzJqjJS1RhikRZohH5/m0x1DeK2na+O7u6Zhx8o7kctruiayGyevnDuA45zPIUR5tQAZ85a1BwDX6BaazgXCw==");
  std::string sig_parsed = std::string(reinterpret_cast<char*>(sigs[0].sig), CRYPTO_SIGNATURE_LEN);
  EXPECT_EQ(sig_in_str, sig_parsed);
}

TEST(tiny_signatures, parse_one_of_two) {
  std::string signatures_str = "[{\"keyid\":\"5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43\",\"method\":\"ed25519\",\"sig\":\"9hU1Bj4mrONkvv8iQHfZnd4obUFVkHHLwPjqoRRquSilZAJMFbAnO/Bx0PBUNfjcdV7xfytw/uNcBdv6lUmrAw==\"},{\"keyid\":\"982daae1f8bc4c81e259112c0baaf3ca49a20bac172044ec735868ec98c3f406\",\"method\":\"ed25519\",\"sig\":\"zrzJqjJS1RhikRZohH5/m0x1DeK2na+O7u6Zhx8o7kctruiayGyevnDuA45zPIUR5tQAZ85a1BwDX6BaazgXCw==\"}]";
  crypto_key_and_signature_t sigs[10];

  EXPECT_EQ(uptane_parse_signatures(ROLE_ROOT, signatures_str.c_str(), signatures_str.length(), sigs, 10), 1);
  EXPECT_TRUE(sigs[0].key != nullptr);
  EXPECT_TRUE(sigs[0].key->key_type == ED25519);
  std::string sig_in_str = Utils::fromBase64("zrzJqjJS1RhikRZohH5/m0x1DeK2na+O7u6Zhx8o7kctruiayGyevnDuA45zPIUR5tQAZ85a1BwDX6BaazgXCw==");
  std::string sig_parsed = std::string(reinterpret_cast<char*>(sigs[0].sig), CRYPTO_SIGNATURE_LEN);
  EXPECT_EQ(sig_in_str, sig_parsed);
}

TEST(tiny_signatures, parse_plus_garbage) {
  std::string signatures_str = "[{\"keyid\":\"5e16c18ad88a82257721d483383468e9a931bc46fe307e991c1c4bc96e62ee43\",\"method\":\"ed25519\",\"sig\":\"9hU1Bj4mrONkvv8iQHfZnd4obUFVkHHLwPjqoRRquSilZAJMFbAnO/Bx0PBUNfjcdV7xfytw/uNcBdv6lUmrAw==\",\"newfield\":{\"subfield1\":\"value\",\"subfield2\":[\"value\",\"value\"]}},{\"keyid\":\"982daae1f8bc4c81e259112c0baaf3ca49a20bac172044ec735868ec98c3f406\",\"method\":\"ed25519\",\"sig\":\"zrzJqjJS1RhikRZohH5/m0x1DeK2na+O7u6Zhx8o7kctruiayGyevnDuA45zPIUR5tQAZ85a1BwDX6BaazgXCw==\",\"newfield\":{\"subfield1\":\"value\",\"subfield2\":[\"value\",\"value\"]}}]";
  crypto_key_and_signature_t sigs[10];

  EXPECT_EQ(uptane_parse_signatures(ROLE_ROOT, signatures_str.c_str(), signatures_str.length(), sigs, 10), 1);
  EXPECT_TRUE(sigs[0].key != nullptr);
  EXPECT_TRUE(sigs[0].key->key_type == ED25519);
  std::string sig_in_str = Utils::fromBase64("zrzJqjJS1RhikRZohH5/m0x1DeK2na+O7u6Zhx8o7kctruiayGyevnDuA45zPIUR5tQAZ85a1BwDX6BaazgXCw==");
  std::string sig_parsed = std::string(reinterpret_cast<char*>(sigs[0].sig), CRYPTO_SIGNATURE_LEN);
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
