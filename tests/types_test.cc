#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"
#include "fsstorage.h"
#include "types.h"
#include "uptane/cryptokey.h"
#include "utils.h"

#ifdef BUILD_P11
#include "p11engine.h"
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

TEST(PackageManagerCredentials, InitFileEmpty) {
  Config config;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  CryptoKey keys(storage, config);
  data::PackageManagerCredentials creds(keys);

  EXPECT_TRUE(boost::filesystem::exists(creds.ca_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.pkey_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.cert_file()));
  EXPECT_TRUE(boost::filesystem::is_empty(creds.ca_file()));
  EXPECT_TRUE(boost::filesystem::is_empty(creds.pkey_file()));
  EXPECT_TRUE(boost::filesystem::is_empty(creds.cert_file()));
}

TEST(PackageManagerCredentials, InitFileValid) {
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
  CryptoKey keys(storage, config);
  data::PackageManagerCredentials creds(keys);

  EXPECT_TRUE(boost::filesystem::exists(creds.ca_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.pkey_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.cert_file()));
  EXPECT_FALSE(boost::filesystem::is_empty(creds.ca_file()));
  EXPECT_FALSE(boost::filesystem::is_empty(creds.pkey_file()));
  EXPECT_FALSE(boost::filesystem::is_empty(creds.cert_file()));
  EXPECT_EQ(ca, Utils::readFile(creds.ca_file()));
  EXPECT_EQ(pkey, Utils::readFile(creds.pkey_file()));
  EXPECT_EQ(cert, Utils::readFile(creds.cert_file()));
}

#ifdef BUILD_P11
TEST(PackageManagerCredentials, InitPkcs11Empty) {
  Config config;
  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "03";
  config.p11 = p11_conf;
  P11Engine p11(p11_conf);

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  CryptoKey keys(storage, config);
  data::PackageManagerCredentials creds(keys);

  EXPECT_TRUE(boost::filesystem::exists(creds.ca_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.pkey_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.cert_file()));
  EXPECT_TRUE(boost::filesystem::is_empty(creds.ca_file()));
  EXPECT_TRUE(boost::filesystem::is_empty(creds.pkey_file()));
  EXPECT_TRUE(boost::filesystem::is_empty(creds.cert_file()));
}

TEST(PackageManagerCredentials, InitPkcs11Valid) {
  Config config;
  P11Config p11_conf;
  p11_conf.module = TEST_PKCS11_MODULE_PATH;
  p11_conf.pass = "1234";
  p11_conf.uptane_key_id = "03";
  config.p11 = p11_conf;
  P11Engine p11(p11_conf);

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  std::string ca = Utils::readFile("tests/test_data/prov/root.crt");
  std::string pkey = Utils::readFile("tests/test_data/prov/pkey.pem");
  std::string cert = Utils::readFile("tests/test_data/prov/client.pem");
  storage->storeTlsCa(ca);
  storage->storeTlsPkey(pkey);
  storage->storeTlsCert(cert);
  CryptoKey keys(storage, config);
  data::PackageManagerCredentials creds(keys);

  EXPECT_TRUE(boost::filesystem::exists(creds.ca_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.pkey_file()));
  EXPECT_TRUE(boost::filesystem::exists(creds.cert_file()));
  EXPECT_FALSE(boost::filesystem::is_empty(creds.ca_file()));
  EXPECT_FALSE(boost::filesystem::is_empty(creds.pkey_file()));
  EXPECT_FALSE(boost::filesystem::is_empty(creds.cert_file()));
  EXPECT_EQ(ca, Utils::readFile(creds.ca_file()));
  EXPECT_EQ(pkey, Utils::readFile(creds.pkey_file()));
  EXPECT_EQ(cert, Utils::readFile(creds.cert_file()));
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
