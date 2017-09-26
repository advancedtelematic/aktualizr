#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <json/json.h>
#include <boost/filesystem.hpp>
#include <string>
#include "boost/algorithm/hex.hpp"

#include "crypto.h"
#include "utils.h"

#ifdef BUILD_P11
#include "p11engine.h"
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

TEST(crypto, sha256_is_correct) {
  std::string test_str = "This is string for testing";
  std::string expected_result = "7DF106BB55506D91E48AF727CD423B169926BA99DF4BAD53AF4D80E717A1AC9F";
  std::string result = boost::algorithm::hex(Crypto::sha256digest(test_str));
  EXPECT_EQ(expected_result, result);
}

TEST(crypto, sha512_is_correct) {
  std::string test_str = "This is string for testing";
  std::string expected_result =
      "D3780CA0200DA69209D204429E034AEA4F661EF20EF38D3F9A0EFA13E1A9E3B37AE4E16308B720B010B6D53D5C020C11B3B7012705C9060F"
      "843D7628FEBC8791";
  std::string result = boost::algorithm::hex(Crypto::sha512digest(test_str));
  EXPECT_EQ(expected_result, result);
}

TEST(crypto, sign_verify_rsa_file) {
  std::string text = "This is text for sign";
  PublicKey pkey(Utils::readFile("tests/test_data/public.key"), "rsa");
  std::string private_key = Utils::readFile("tests/test_data/priv.key");
  std::string signature = Utils::toBase64(Crypto::RSAPSSSign(NULL, private_key, text));
  bool signe_is_ok = Crypto::VerifySignature(pkey, signature, text);
  EXPECT_TRUE(signe_is_ok);
}

#ifdef BUILD_P11
TEST(crypto, sign_verify_rsa_p11) {
  P11Config config;
  config.module = TEST_PKCS11_MODULE_PATH;
  config.pass = "1234";

  P11Engine p11(config);
  std::string text = "This is text for sign";
  std::string key_content;
  EXPECT_TRUE(p11.readPublicKey("03", &key_content));
  PublicKey pkey(key_content, "rsa");
  std::string private_key = p11.getUriPrefix() + "03";
  std::string signature = Utils::toBase64(Crypto::RSAPSSSign(p11.getEngine(), private_key, text));
  bool signe_is_ok = Crypto::VerifySignature(pkey, signature, text);
  EXPECT_TRUE(signe_is_ok);
}
#endif

TEST(crypto, sign_bad_key_no_crash) {
  std::string text = "This is text for sign";
  std::string signature = Utils::toBase64(Crypto::RSAPSSSign(NULL, "this is bad key path", text));
  EXPECT_TRUE(signature.empty());
}

TEST(crypto, verify_bad_key_no_crash) {
  std::string text = "This is text for sign";
  std::string signature = Utils::toBase64(Crypto::RSAPSSSign(NULL, "tests/test_data/priv.key", text));
  bool signe_is_ok = Crypto::RSAPSSVerify("this is bad key", signature, text);
  EXPECT_EQ(signe_is_ok, false);
}

TEST(crypto, verify_bad_sign_no_crash) {
  PublicKey pkey(Utils::readFile("tests/test_data/public.key"), "rsa");
  std::string text = "This is text for sign";
  bool signe_is_ok = Crypto::VerifySignature(pkey, "this is bad signature", text);
  EXPECT_EQ(signe_is_ok, false);
}

TEST(crypto, verify_ed25519) {
  std::ifstream root_stream("tests/test_data/ed25519_signed.json");
  std::string text((std::istreambuf_iterator<char>(root_stream)), std::istreambuf_iterator<char>());
  root_stream.close();
  std::string signature = "lS1GII6MS2FAPuSzBPHOZbE0wLIRpFhlbaCSgNOJLT1h+69OjaN/YQq16uzoXX3rev/Dhw0Raa4v9xocE8GmBA==";
  PublicKey pkey("cb07563157805c279ec90ccb057f2c3ea6e89200e1e67f8ae66185987ded9b1c", "ed25519");
  bool signe_is_ok = Crypto::VerifySignature(pkey, signature, Json::FastWriter().write(Utils::parseJSON(text)));
  EXPECT_TRUE(signe_is_ok);

  std::string signature_bad =
      "33lS1GII6MS2FAPuSzBPHOZbE0wLIRpFhlbaCSgNOJLT1h+69OjaN/YQq16uzoXX3rev/Dhw0Raa4v9xocE8GmBA==";
  signe_is_ok = Crypto::VerifySignature(pkey, signature_bad, Json::FastWriter().write(Utils::parseJSON(text)));
  EXPECT_FALSE(signe_is_ok);
}

TEST(crypto, bad_keytype) {
  try {
    PublicKey pkey("somekey", "nosuchtype");
    FAIL();
  } catch (std::runtime_error ex) {
  }
}

TEST(crypto, parsep12) {
  std::string pkey;
  std::string cert;
  std::string ca;

  FILE *p12file = fopen("tests/test_data/cred.p12", "rb");
  if (!p12file) {
    EXPECT_TRUE(false) << " could not open tests/test_data/cred.p12";
    std::cout << "fdgdgdfg\n\n\n";
  }
  Crypto::parseP12(p12file, "", &pkey, &cert, &ca);
  fclose(p12file);
  // TODO: expected values
  EXPECT_EQ(!pkey.empty(), true);
  EXPECT_EQ(!cert.empty(), true);
  EXPECT_EQ(!ca.empty(), true);
}

TEST(crypto, parsep12_FAIL) {
  std::string pkey;
  std::string cert;
  std::string ca;

  FILE *bad_p12file = fopen("tests/test_data/priv.key", "rb");
  if (!bad_p12file) {
    EXPECT_TRUE(false) << " could not open tests/test_data/priv.key";
  }
  bool result = Crypto::parseP12(bad_p12file, "", &pkey, &cert, &ca);
  fclose(bad_p12file);
  EXPECT_EQ(result, false);
}

TEST(crypto, generateRSAKeyPair) {
  std::string public_key;
  std::string private_key;
  // Generation should succeed
  EXPECT_TRUE(Crypto::generateRSAKeyPair(&public_key, &private_key));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
