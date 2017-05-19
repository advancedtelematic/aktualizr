#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <string>
#include "boost/algorithm/hex.hpp"
#include <json/json.h>

#include "crypto.h"
#include "utils.h"

TEST(crypto, sha256_is_correct) {
  std::string test_str = "This is string for testing";
  std::string expected_result = "7DF106BB55506D91E48AF727CD423B169926BA99DF4BAD53AF4D80E717A1AC9F";
  std::string result = boost::algorithm::hex(Crypto::sha256digest(test_str));
  EXPECT_EQ(expected_result, result);
}

TEST(crypto, sign_verify_rsa) {
  std::ifstream path_stream("tests/test_data/public.key");
  std::string public_key((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  path_stream.close();
  std::string text = "This is text for sign";
  PublicKey pkey(public_key, "rsa");
  std::string signature = Utils::toBase64(Crypto::RSAPSSSign("tests/test_data/priv.key", text));
  bool signe_is_ok = Crypto::VerifySignature(pkey, signature, text);
  EXPECT_TRUE(signe_is_ok);
}

TEST(crypto, verify_ed25519) {
  std::ifstream root_stream("tests/test_data/ed25519_signed.json");
  std::string text((std::istreambuf_iterator<char>(root_stream)), std::istreambuf_iterator<char>());
  root_stream.close();
  std::string signature =
      "lS1GII6MS2FAPuSzBPHOZbE0wLIRpFhlbaCSgNOJLT1h+69OjaN/YQq16uzoXX3rev/Dhw0Raa4v9xocE8GmBA==";
  PublicKey pkey("cb07563157805c279ec90ccb057f2c3ea6e89200e1e67f8ae66185987ded9b1c", "ed25519");
  bool signe_is_ok = Crypto::VerifySignature(pkey, signature, Json::FastWriter().write(Utils::parseJSON(text)));
  EXPECT_TRUE(signe_is_ok);

  std::string signature_bad =
      "33lS1GII6MS2FAPuSzBPHOZbE0wLIRpFhlbaCSgNOJLT1h+69OjaN/YQq16uzoXX3rev/Dhw0Raa4v9xocE8GmBA==";
  signe_is_ok = Crypto::VerifySignature(pkey, signature_bad, Json::FastWriter().write(Utils::parseJSON(text)));
  EXPECT_FALSE(signe_is_ok);
}

TEST(crypto, parsep12) {
  std::string pkey_file = "/tmp/aktualizr_pkey_test.pem";
  std::string cert_file = "/tmp/aktualizr_cert_test.pem";
  std::string ca_file = "/tmp/aktualizr_ca_test.pem";

  FILE *p12file = fopen("tests/test_data/cred.p12", "rb");
  if (!p12file) {
    EXPECT_TRUE(false) << " could not open tests/test_data/cred.p12";
    std::cout << "fdgdgdfg\n\n\n";
  }
  Crypto::parseP12(p12file, "", pkey_file, cert_file, ca_file);
  fclose(p12file);
  EXPECT_EQ(boost::filesystem::exists(pkey_file), true);
  EXPECT_EQ(boost::filesystem::exists(cert_file), true);
  EXPECT_EQ(boost::filesystem::exists(ca_file), true);
}

TEST(crypto, parsep12_FAIL) {
  std::string pkey_file = "/tmp/aktualizr_pkey_test.pem";
  std::string cert_file = "/tmp/aktualizr_cert_test.pem";
  std::string ca_file = "/tmp/aktualizr_ca_test.pem";

  FILE *p12file = fopen("tests/test_data/cred.p12", "rb");
  if (!p12file) {
    EXPECT_TRUE(false) << " could not open tests/test_data/cred.p12";
  }
  bool result = Crypto::parseP12(p12file, "", "", cert_file, ca_file);
  EXPECT_EQ(result, false);
  fclose(p12file);

  p12file = fopen("tests/test_data/cred.p12", "rb");
  result = Crypto::parseP12(p12file, "", pkey_file, "", ca_file);
  EXPECT_EQ(result, false);
  fclose(p12file);

  p12file = fopen("tests/test_data/cred.p12", "rb");
  result = Crypto::parseP12(p12file, "", pkey_file, cert_file, "");
  EXPECT_EQ(result, false);
  fclose(p12file);

  p12file = fopen("tests/test_data/data.txt", "rb");
  result = Crypto::parseP12(p12file, "", pkey_file, cert_file, "");
  EXPECT_EQ(result, false);
  fclose(p12file);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
