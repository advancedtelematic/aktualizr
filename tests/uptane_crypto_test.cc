#include <gtest/gtest.h>

#include <boost/make_shared.hpp>

#include "fsstorage.h"
#include "uptane/cryptokey.h"

TEST(crypto, sign_tuf) {
  std::string private_key = Utils::readFile("tests/test_data/priv.key");
  std::string public_key = Utils::readFile("tests/test_data/public.key");
  Config config;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  storage->storePrimaryKeys(public_key, private_key);
  CryptoKey keys(storage, config);

  Json::Value tosign_json;
  tosign_json["mykey"] = "value";
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}

#ifdef BUILD_P11
TEST(crypto, sign_tuf_pkcs11) {
  std::string private_key = Utils::readFile("tests/test_data/priv.key");
  std::string public_key = Utils::readFile("tests/test_data/public.key");

  Json::Value tosign_json;
  tosign_json["mykey"] = "value";

  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "03";
  Config config;
  config.p11 = p11_conf;

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  storage->storePrimaryKeys(public_key, private_key);
  CryptoKey keys(storage, config);

  EXPECT_TRUE(keys.getUptanePublicKey().size());
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
