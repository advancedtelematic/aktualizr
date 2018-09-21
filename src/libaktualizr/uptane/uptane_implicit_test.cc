/**
 * \file
 */

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/smart_ptr/make_shared.hpp>

#include "httpfake.h"
#include "logging/logging.h"
#include "primary/initializer.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

/**
 * Verify that when using implicit provisioning, aktualizr halts if credentials
 * are not available.
 */
TEST(UptaneImplicit, ImplicitFailure) {
  RecordProperty("zephyr_key", "REQ-345,TST-185");
  Config config;
  config.provision.device_id = "device_id";

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  KeyManager keys(storage, config.keymanagerConfig());

  Initializer initializer(config.provision, storage, http, keys, {});
  EXPECT_FALSE(initializer.isSuccessful());
}

/**
 * Verfiy that aktualizr halts when provided incomplete implicit provisioning
 * credentials.
 */
TEST(UptaneImplicit, ImplicitIncomplete) {
  RecordProperty("zephyr_key", "REQ-345,TST-187");
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.provision.device_id = "device_id";
  config.postUpdateValues();

  auto http = std::make_shared<HttpFake>(temp_dir.Path());

  {
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir.Path() / "ca.pem");
    auto storage = INvStorage::newStorage(config.storage);
    KeyManager keys(storage, config.keymanagerConfig());

    Initializer initializer(config.provision, storage, http, keys, {});
    EXPECT_FALSE(initializer.isSuccessful());
  }

  {
    boost::filesystem::remove_all(temp_dir.Path());
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir.Path() / "client.pem");
    auto storage = INvStorage::newStorage(config.storage);
    KeyManager keys(storage, config.keymanagerConfig());

    Initializer initializer(config.provision, storage, http, keys, {});
    EXPECT_FALSE(initializer.isSuccessful());
  }

  {
    boost::filesystem::remove_all(temp_dir.Path());
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir.Path() / "pkey.pem");
    auto storage = INvStorage::newStorage(config.storage);
    KeyManager keys(storage, config.keymanagerConfig());

    Initializer initializer(config.provision, storage, http, keys, {});
    EXPECT_FALSE(initializer.isSuccessful());
  }

  {
    boost::filesystem::remove_all(temp_dir.Path());
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir.Path() / "ca.pem");
    boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir.Path() / "client.pem");
    auto storage = INvStorage::newStorage(config.storage);
    KeyManager keys(storage, config.keymanagerConfig());

    Initializer initializer(config.provision, storage, http, keys, {});
    EXPECT_FALSE(initializer.isSuccessful());
  }

  {
    boost::filesystem::remove_all(temp_dir.Path());
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir.Path() / "ca.pem");
    boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir.Path() / "pkey.pem");
    auto storage = INvStorage::newStorage(config.storage);
    KeyManager keys(storage, config.keymanagerConfig());

    Initializer initializer(config.provision, storage, http, keys, {});
    EXPECT_FALSE(initializer.isSuccessful());
  }

  {
    boost::filesystem::remove_all(temp_dir.Path());
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir.Path() / "client.pem");
    boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir.Path() / "pkey.pem");
    auto storage = INvStorage::newStorage(config.storage);
    KeyManager keys(storage, config.keymanagerConfig());

    Initializer initializer(config.provision, storage, http, keys, {});
    EXPECT_FALSE(initializer.isSuccessful());
  }
}

/**
 * Verify that aktualizr can implicitly provision with provided credentials.
 */
TEST(UptaneImplicit, ImplicitProvision) {
  RecordProperty("zephyr_key", "OTA-996,REQ-349,TST-186");
  Config config;
  TemporaryDirectory temp_dir;
  Utils::createDirectories(temp_dir / "import", S_IRWXU);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir / "import/ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir / "import/client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir / "import/pkey.pem");
  config.storage.path = temp_dir.Path();
  config.import.base_path = temp_dir / "import";
  config.import.tls_cacert_path = BasedPath("ca.pem");
  config.import.tls_clientcert_path = BasedPath("client.pem");
  config.import.tls_pkey_path = BasedPath("pkey.pem");

  auto storage = INvStorage::newStorage(config.storage);
  storage->importData(config.import);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  KeyManager keys(storage, config.keymanagerConfig());

  Initializer initializer(config.provision, storage, http, keys, {});
  EXPECT_TRUE(initializer.isSuccessful());
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
