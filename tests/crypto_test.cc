#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <string>

#include <json/json.h>
#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>

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

TEST(crypto, sign_tuf) {
  Config config;

  Json::Value tosign_json;
  tosign_json["mykey"] = "value";

  std::string private_key = Utils::readFile("tests/test_data/priv.key");

  std::string public_key = Utils::readFile("tests/test_data/public.key");
  Json::Value signed_json = Crypto::signTuf(NULL, private_key, Crypto::getKeyId(public_key), tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}

#ifdef BUILD_P11
TEST(crypto, sign_verify_rsa_p11) {
  P11Config config;
  config.module = TEST_PKCS11_MODULE_PATH;
  config.pass = "1234";
  config.uptane_key_id = "03";

  P11Engine p11(config);
  std::string text = "This is text for sign";
  std::string key_content;
  EXPECT_TRUE(p11.readUptanePublicKey(&key_content));
  PublicKey pkey(key_content, "rsa");
  std::string private_key = p11.getUptaneKeyId();
  std::string signature = Utils::toBase64(Crypto::RSAPSSSign(p11.getEngine(), private_key, text));
  bool signe_is_ok = Crypto::VerifySignature(pkey, signature, text);
  EXPECT_TRUE(signe_is_ok);
}

TEST(crypto, generate_rsa_keypair_p11) {
  P11Config config;
  config.module = TEST_PKCS11_MODULE_PATH;
  config.pass = "1234";
  config.uptane_key_id = "05";

  P11Engine p11(config);
  std::string key_content;
  EXPECT_FALSE(p11.readUptanePublicKey(&key_content));
  EXPECT_TRUE(p11.generateUptaneKeyPair());
  EXPECT_TRUE(p11.readUptanePublicKey(&key_content));
}

TEST(crypto, sign_tuf_pkcs11) {
  Json::Value tosign_json;
  tosign_json["mykey"] = "value";

  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "03";
  P11Engine p11(p11_conf);

  std::string public_key;
  EXPECT_TRUE(p11.readUptanePublicKey(&public_key));
  Json::Value signed_json =
      Crypto::signTuf(p11.getEngine(), p11.getUptaneKeyId(), Crypto::getKeyId(public_key), tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}

TEST(crypto, certificate_pkcs11) {
  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.tls_clientcert_id = "01";
  P11Engine p11(p11_conf);

  std::string cert;
  bool res = p11.readTlsCert(&cert);
  EXPECT_TRUE(res);
  if (!res) return;

  std::string device_name;
  EXPECT_TRUE(Crypto::extractSubjectCN(cert, &device_name));
  EXPECT_EQ(device_name, "cc34f7f3-481d-443b-bceb-e838a36a2d1f");
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
