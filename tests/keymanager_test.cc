#include <gtest/gtest.h>

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include "json/json.h"

#include "config.h"
#include "fsstorage.h"
#include "keymanager.h"
#include "utils.h"

#ifdef BUILD_P11
#include "p11engine.h"
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

TEST(KeyManager, SignTuf) {
  std::string private_key = Utils::readFile("tests/test_data/priv.key");
  std::string public_key = Utils::readFile("tests/test_data/public.key");
  Config config;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  storage->storePrimaryKeys(public_key, private_key);
  KeyManager keys(storage, config);

  Json::Value tosign_json;
  tosign_json["mykey"] = "value";
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}

TEST(KeyManager, InitFileEmpty) {
  Config config;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  KeyManager keys(storage, config);

  EXPECT_TRUE(keys.getCaFile().empty());
  EXPECT_TRUE(keys.getPkeyFile().empty());
  EXPECT_TRUE(keys.getCertFile().empty());
  keys.loadKeys();
  EXPECT_TRUE(keys.getCaFile().empty());
  EXPECT_TRUE(keys.getPkeyFile().empty());
  EXPECT_TRUE(keys.getCertFile().empty());
}

TEST(KeyManager, InitFileValid) {
  Config config;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  KeyManager keys(storage, config);
  keys.loadKeys();

  std::string ca_file = keys.getCaFile();
  std::string pkey_file = keys.getPkeyFile();
  std::string cert_file = keys.getCertFile();

  EXPECT_TRUE(boost::filesystem::exists(ca_file));
  EXPECT_TRUE(boost::filesystem::exists(pkey_file));
  EXPECT_TRUE(boost::filesystem::exists(cert_file));
  EXPECT_FALSE(boost::filesystem::is_empty(ca_file));
  EXPECT_FALSE(boost::filesystem::is_empty(pkey_file));
  EXPECT_FALSE(boost::filesystem::is_empty(cert_file));
  EXPECT_EQ(ca, Utils::readFile(ca_file));
  EXPECT_EQ(pkey, Utils::readFile(pkey_file));
  EXPECT_EQ(cert, Utils::readFile(cert_file));
}

#ifdef BUILD_P11
TEST(KeyManager, SignTufPkcs11) {
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
  KeyManager keys(storage, config);

  EXPECT_TRUE(keys.getUptanePublicKey().size());
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}

TEST(KeyManager, InitPkcs11Empty) {
  Config config;
  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "03";
  config.p11 = p11_conf;

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  KeyManager keys(storage, config);

  EXPECT_TRUE(keys.getCaFile().empty());
  EXPECT_TRUE(keys.getPkeyFile().empty());
  EXPECT_TRUE(keys.getCertFile().empty());
  keys.loadKeys();
  EXPECT_TRUE(keys.getCaFile().empty());
  EXPECT_TRUE(keys.getPkeyFile().empty());
  EXPECT_TRUE(keys.getCertFile().empty());
}

TEST(KeyManager, InitPkcs11Valid) {
  Config config;
  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "03";
  config.p11 = p11_conf;

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  KeyManager keys(storage, config);
  EXPECT_FALSE(keys.getCaFile().empty());
  EXPECT_FALSE(keys.getPkeyFile().empty());
  EXPECT_FALSE(keys.getCertFile().empty());
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
