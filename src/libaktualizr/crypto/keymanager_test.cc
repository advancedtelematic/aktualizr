#include <gtest/gtest.h>
#include <memory>

#include "json/json.h"

#include "crypto/keymanager.h"
#include "libaktualizr/config.h"
#include "storage/sqlstorage.h"
#include "utilities/utils.h"

#ifdef BUILD_P11
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

/* Sign TUF metadata with RSA2048. */
TEST(KeyManager, SignTuf) {
  std::string private_key = Utils::readFile("tests/test_data/priv.key");
  std::string public_key = Utils::readFile("tests/test_data/public.key");
  Config config;
  config.uptane.key_type = KeyType::kRSA2048;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  storage->storePrimaryKeys(public_key, private_key);
  KeyManager keys(storage, config.keymanagerConfig());

  Json::Value tosign_json;
  tosign_json["mykey"] = "value";
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_NE(signed_json["signatures"][0]["sig"].asString().size(), 0);
  EXPECT_EQ(signed_json["signatures"][0]["method"].asString(), "rsassa-pss");
}

/* Sign TUF metadata with ED25519. */
TEST(KeyManager, SignED25519Tuf) {
  std::string private_key =
      "BD0A7539BD0365D7A9A3050390AD7B7C2033C58E354C5E0F42B9B611273BBA38BB9FFA4DCF35A89F6F40C5FA67998DD38B64A8459598CF3D"
      "A93853388FDAC760";
  std::string public_key = "BB9FFA4DCF35A89F6F40C5FA67998DD38B64A8459598CF3DA93853388FDAC760";
  Config config;
  config.uptane.key_type = KeyType::kED25519;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);

  storage->storePrimaryKeys(public_key, private_key);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.loadKeys();

  Json::Value tosign_json;
  tosign_json["mykey"] = "value";
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "a6d0f6b52ae833175dd7724899507709231723037845715c7677670e0195f850");
  EXPECT_NE(signed_json["signatures"][0]["sig"].asString().size(), 0);
  EXPECT_EQ(signed_json["signatures"][0]["method"].asString(), "ed25519");
}

TEST(KeyManager, InitFileEmpty) {
  Config config;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());

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
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  KeyManager keys(storage, config.keymanagerConfig());

  EXPECT_TRUE(keys.getCaFile().empty());
  EXPECT_TRUE(keys.getPkeyFile().empty());
  EXPECT_TRUE(keys.getCertFile().empty());
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
/* Sign and verify a file with RSA via PKCS#11. */
TEST(KeyManager, SignTufPkcs11) {
  Json::Value tosign_json;
  tosign_json["mykey"] = "value";

  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "03";
  Config config;
  config.p11 = p11_conf;
  config.uptane.key_source = CryptoSource::kPkcs11;

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());

  EXPECT_GT(keys.UptanePublicKey().Value().size(), 0);
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_NE(signed_json["signatures"][0]["sig"].asString().size(), 0);
}

/* Generate Uptane keys, use them for signing, and verify them. */
TEST(KeyManager, GenSignTufPkcs11) {
  Json::Value tosign_json;
  tosign_json["mykey"] = "value";

  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "06";
  Config config;
  config.p11 = p11_conf;
  config.uptane.key_source = CryptoSource::kPkcs11;

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());

  P11EngineGuard p11(config.p11);
  EXPECT_TRUE(p11->generateUptaneKeyPair());

  EXPECT_GT(keys.UptanePublicKey().Value().size(), 0);
  Json::Value signed_json = keys.signTuf(tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_NE(signed_json["signatures"][0]["sig"].asString().size(), 0);
}

/* Generate RSA keypairs via PKCS#11. */
TEST(KeyManager, InitPkcs11Valid) {
  Config config;
  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.tls_pkey_id = "02";
  p11_conf.tls_clientcert_id = "01";
  config.p11 = p11_conf;
  config.tls.ca_source = CryptoSource::kFile;
  config.tls.pkey_source = CryptoSource::kPkcs11;
  config.tls.cert_source = CryptoSource::kPkcs11;

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  // Getting the CA from the HSM is not currently supported.
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  storage->storeTlsCa(ca);
  KeyManager keys(storage, config.keymanagerConfig());
  EXPECT_TRUE(keys.getCaFile().empty());
  EXPECT_FALSE(keys.getPkeyFile().empty());
  EXPECT_FALSE(keys.getCertFile().empty());
  keys.loadKeys();
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
