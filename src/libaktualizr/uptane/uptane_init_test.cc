#include <gtest/gtest.h>

#include <string>

#include <boost/filesystem.hpp>

#include "httpfake.h"
#include "primary/initializer.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

/*
 * Check that aktualizr creates provisioning files if they don't exist already.
 */
TEST(Uptane, Initialize) {
  RecordProperty("zephyr_key", "OTA-983,TST-153");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf("tests/config/basic.toml");
  conf.uptane.repo_server = http->tls_server + "/director";

  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.tls.server = http->tls_server;

  conf.storage.path = temp_dir.Path();
  conf.provision.primary_ecu_serial = "testecuserial";

  auto storage = INvStorage::newStorage(conf.storage);
  std::string pkey;
  std::string cert;
  std::string ca;
  bool result = storage->loadTlsCreds(&ca, &cert, &pkey);
  EXPECT_FALSE(result);

  KeyManager keys(storage, conf.keymanagerConfig());
  Initializer initializer(conf.provision, storage, http, keys, {});

  result = initializer.isSuccessful();
  EXPECT_TRUE(result);

  result = storage->loadTlsCreds(&ca, &cert, &pkey);
  EXPECT_TRUE(result);
  EXPECT_NE(ca, "");
  EXPECT_NE(cert, "");
  EXPECT_NE(pkey, "");

  Json::Value ecu_data = Utils::parseJSONFile(temp_dir.Path() / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 1);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), conf.provision.primary_ecu_serial);
}

/*
 * Check that aktualizr does NOT change provisioning files if they DO exist
 * already.
 */
TEST(Uptane, InitializeTwice) {
  RecordProperty("zephyr_key", "OTA-983,TST-154");
  TemporaryDirectory temp_dir;
  Config conf("tests/config/basic.toml");
  conf.storage.path = temp_dir.Path();
  conf.provision.primary_ecu_serial = "testecuserial";

  auto storage = INvStorage::newStorage(conf.storage);
  std::string pkey1;
  std::string cert1;
  std::string ca1;
  bool result = storage->loadTlsCreds(&ca1, &cert1, &pkey1);
  EXPECT_FALSE(result);

  auto http = std::make_shared<HttpFake>(temp_dir.Path());

  {
    KeyManager keys(storage, conf.keymanagerConfig());
    Initializer initializer(conf.provision, storage, http, keys, {});

    result = initializer.isSuccessful();
    EXPECT_TRUE(result);

    result = storage->loadTlsCreds(&ca1, &cert1, &pkey1);
    EXPECT_TRUE(result);
    EXPECT_NE(ca1, "");
    EXPECT_NE(cert1, "");
    EXPECT_NE(pkey1, "");
  }

  {
    KeyManager keys(storage, conf.keymanagerConfig());
    Initializer initializer(conf.provision, storage, http, keys, {});

    result = initializer.isSuccessful();
    EXPECT_TRUE(result);

    std::string pkey2;
    std::string cert2;
    std::string ca2;
    result = storage->loadTlsCreds(&ca2, &cert2, &pkey2);
    EXPECT_TRUE(result);

    EXPECT_EQ(cert1, cert2);
    EXPECT_EQ(ca1, ca2);
    EXPECT_EQ(pkey1, pkey2);
  }
}

/**
 * Check that aktualizr does not generate a pet name when device ID is
 * specified. This is currently provisional and not a finalized requirement at
 * this time.
 */
TEST(Uptane, PetNameProvided) {
  RecordProperty("zephyr_key", "OTA-985,TST-146");
  TemporaryDirectory temp_dir;
  std::string test_name = "test-name-123";
  boost::filesystem::path device_path = temp_dir.Path() / "device_id";

  /* Make sure provided device ID is read as expected. */
  Config conf("tests/config/device_id.toml");
  conf.storage.path = temp_dir.Path();
  conf.provision.primary_ecu_serial = "testecuserial";

  auto storage = INvStorage::newStorage(conf.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  KeyManager keys(storage, conf.keymanagerConfig());
  Initializer initializer(conf.provision, storage, http, keys, {});

  EXPECT_TRUE(initializer.isSuccessful());

  {
    EXPECT_EQ(conf.provision.device_id, test_name);
    std::string devid;
    EXPECT_TRUE(storage->loadDeviceId(&devid));
    EXPECT_EQ(devid, test_name);
  }

  {
    /* Make sure name is unchanged after re-initializing config. */
    conf.postUpdateValues();
    EXPECT_EQ(conf.provision.device_id, test_name);
    std::string devid;
    EXPECT_TRUE(storage->loadDeviceId(&devid));
    EXPECT_EQ(devid, test_name);
  }
}

/**
 * Check that aktualizr generates a pet name if no device ID is specified.
 */
TEST(Uptane, PetNameCreation) {
  RecordProperty("zephyr_key", "OTA-985,TST-145");
  TemporaryDirectory temp_dir;
  boost::filesystem::path device_path = temp_dir.Path() / "device_id";

  // Make sure name is created.
  Config conf("tests/config/basic.toml");
  conf.storage.path = temp_dir.Path();
  conf.provision.primary_ecu_serial = "testecuserial";
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir.Path() / "cred.zip");
  conf.provision.provision_path = temp_dir.Path() / "cred.zip";

  std::string test_name1, test_name2;
  {
    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());

    KeyManager keys(storage, conf.keymanagerConfig());
    Initializer initializer(conf.provision, storage, http, keys, {});

    EXPECT_TRUE(initializer.isSuccessful());

    EXPECT_TRUE(storage->loadDeviceId(&test_name1));
    EXPECT_NE(test_name1, "");
  }

  // Make sure a new name is generated if the config does not specify a name and
  // there is no device_id file.
  TemporaryDirectory temp_dir2;
  {
    conf.storage.path = temp_dir2.Path();
    boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir2.Path() / "cred.zip");
    conf.provision.device_id = "";

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    KeyManager keys(storage, conf.keymanagerConfig());
    Initializer initializer(conf.provision, storage, http, keys, {});

    EXPECT_TRUE(initializer.isSuccessful());

    EXPECT_TRUE(storage->loadDeviceId(&test_name2));
    EXPECT_NE(test_name2, test_name1);
  }

  // If the device_id is cleared in the config, but the file is still present,
  // re-initializing the config should still read the device_id from file.
  {
    conf.provision.device_id = "";
    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    KeyManager keys(storage, conf.keymanagerConfig());
    Initializer initializer(conf.provision, storage, http, keys, {});

    EXPECT_TRUE(initializer.isSuccessful());

    std::string devid;
    EXPECT_TRUE(storage->loadDeviceId(&devid));
    EXPECT_EQ(devid, test_name2);
  }

  // If the device_id file is removed, but the field is still present in the
  // config, re-initializing the config should still read the device_id from
  // config.
  {
    TemporaryDirectory temp_dir3;
    conf.storage.path = temp_dir3.Path();
    boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir3.Path() / "cred.zip");
    conf.provision.device_id = test_name2;

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    KeyManager keys(storage, conf.keymanagerConfig());
    Initializer initializer(conf.provision, storage, http, keys, {});

    EXPECT_TRUE(initializer.isSuccessful());

    std::string devid;
    EXPECT_TRUE(storage->loadDeviceId(&devid));
    EXPECT_EQ(devid, test_name2);
  }
}

TEST(Uptane, InitializeFail) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf("tests/config/basic.toml");
  conf.uptane.repo_server = http->tls_server + "/director";
  conf.storage.path = temp_dir.Path();

  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.tls.server = http->tls_server;

  conf.provision.primary_ecu_serial = "testecuserial";

  auto storage = INvStorage::newStorage(conf.storage);

  http->provisioningResponse = ProvisioningResult::kFailure;
  KeyManager keys(storage, conf.keymanagerConfig());
  Initializer initializer(conf.provision, storage, http, keys, {});

  bool result = initializer.isSuccessful();
  http->provisioningResponse = ProvisioningResult::kOK;
  EXPECT_FALSE(result);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
