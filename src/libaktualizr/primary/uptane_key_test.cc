#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/polymorphic_pointer_cast.hpp>

#include "httpfake.h"
#include "logging/logging.h"
#include "managedsecondary.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "uptane/uptanerepository.h"
#include "uptane_test_common.h"

void initKeyTests(Config& config, Primary::VirtualSecondaryConfig& ecu_config1,
                  Primary::VirtualSecondaryConfig& ecu_config2, TemporaryDirectory& temp_dir,
                  const std::string& tls_server) {
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.primary_ecu_serial = "testecuserial";
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kSharedCred;
  config.tls.server = tls_server;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.storage.path = temp_dir.Path();
  config.pacman.type = PACKAGE_MANAGER_NONE;

  ecu_config1.partial_verifying = false;
  ecu_config1.full_client_dir = temp_dir.Path();
  ecu_config1.ecu_serial = "secondary_ecu_serial1";
  ecu_config1.ecu_hardware_id = "secondary_hardware1";
  ecu_config1.ecu_private_key = "sec1.priv";
  ecu_config1.ecu_public_key = "sec1.pub";
  ecu_config1.firmware_path = temp_dir / "firmware1.txt";
  ecu_config1.target_name_path = temp_dir / "firmware1_name.txt";
  ecu_config1.metadata_path = temp_dir / "secondary1_metadata";

  ecu_config2.partial_verifying = false;
  ecu_config2.full_client_dir = temp_dir.Path();
  ecu_config2.ecu_serial = "secondary_ecu_serial2";
  ecu_config2.ecu_hardware_id = "secondary_hardware2";
  ecu_config2.ecu_private_key = "sec2.priv";
  ecu_config2.ecu_public_key = "sec2.pub";
  ecu_config2.firmware_path = temp_dir / "firmware2.txt";
  ecu_config2.target_name_path = temp_dir / "firmware2_name.txt";
  ecu_config2.metadata_path = temp_dir / "secondary2_metadata";
}

// This is a class solely for the purpose of being a FRIEND_TEST to
// SotaUptaneClient. The name is carefully constructed for this purpose.
class UptaneKey_Check_Test {
 public:
  static void checkKeyTests(std::shared_ptr<INvStorage>& storage, const SotaUptaneClient& sota_client) {
    // Verify that TLS credentials are valid.
    std::string ca;
    std::string cert;
    std::string pkey;
    EXPECT_TRUE(storage->loadTlsCreds(&ca, &cert, &pkey));
    EXPECT_GT(ca.size(), 0);
    EXPECT_GT(cert.size(), 0);
    EXPECT_GT(pkey.size(), 0);

    // Verify that Primary keys are valid.
    std::string primary_public;
    std::string primary_private;
    EXPECT_TRUE(storage->loadPrimaryKeys(&primary_public, &primary_private));
    EXPECT_GT(primary_public.size(), 0);
    EXPECT_GT(primary_private.size(), 0);

    EcuSerials ecu_serials;
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials));
    EXPECT_EQ(ecu_serials.size(), 3);

    std::vector<std::string> public_keys;
    std::vector<std::string> private_keys;
    public_keys.push_back(primary_public);
    private_keys.push_back(primary_private);

    // Verify that each Secondary has valid keys.
    for (auto it = sota_client.secondaries.begin(); it != sota_client.secondaries.end(); it++) {
      std::shared_ptr<Primary::ManagedSecondary> managed =
          boost::polymorphic_pointer_downcast<Primary::ManagedSecondary>(it->second);
      std::string public_key;
      std::string private_key;
      EXPECT_TRUE(managed->loadKeys(&public_key, &private_key));
      EXPECT_GT(public_key.size(), 0);
      EXPECT_GT(private_key.size(), 0);
      EXPECT_NE(public_key, private_key);
      public_keys.push_back(public_key);
      private_keys.push_back(private_key);
    }

    // Verify that none of the ECUs have matching keys.
    std::sort(public_keys.begin(), public_keys.end());
    EXPECT_EQ(adjacent_find(public_keys.begin(), public_keys.end()), public_keys.end());
    std::sort(private_keys.begin(), private_keys.end());
    EXPECT_EQ(adjacent_find(private_keys.begin(), private_keys.end()), private_keys.end());
  }
};

/**
 * Check that all keys are present after successful provisioning.
 */
TEST(UptaneKey, CheckAllKeys) {
  RecordProperty("zephyr_key", "OTA-987,TST-159");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  Primary::VirtualSecondaryConfig ecu_config1;
  Primary::VirtualSecondaryConfig ecu_config2;
  initKeyTests(config, ecu_config1, ecu_config2, temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  ImageReader image_reader = std::bind(&INvStorage::openTargetFile, storage.get(), std::placeholders::_1);
  sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config1, image_reader));
  sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config2, image_reader));
  EXPECT_NO_THROW(sota_client->initialize());
  UptaneKey_Check_Test::checkKeyTests(storage, *sota_client);
}

/**
 * Check that aktualizr can recover from a half done device registration.
 */
TEST(UptaneKey, RecoverWithoutKeys) {
  RecordProperty("zephyr_key", "OTA-987,TST-160");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  Primary::VirtualSecondaryConfig ecu_config1;
  Primary::VirtualSecondaryConfig ecu_config2;

  initKeyTests(config, ecu_config1, ecu_config2, temp_dir, http->tls_server);

  // Initialize.
  {
    auto storage = INvStorage::newStorage(config.storage);
    auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
    ImageReader image_reader = std::bind(&INvStorage::openTargetFile, storage.get(), std::placeholders::_1);
    sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config1, image_reader));
    sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config2, image_reader));
    EXPECT_NO_THROW(sota_client->initialize());
    UptaneKey_Check_Test::checkKeyTests(storage, *sota_client);

    // Remove TLS keys but keep ECU keys and try to initialize again.
    storage->clearTlsCreds();
  }

  {
    auto storage = INvStorage::newStorage(config.storage);
    auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
    sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config1));
    sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config2));
    EXPECT_NO_THROW(sota_client->initialize());
    UptaneKey_Check_Test::checkKeyTests(storage, *sota_client);

    // Remove ECU keys but keep TLS keys and try to initialize again.
    storage->clearPrimaryKeys();
  }

  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_public_key);
  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_private_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_public_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_private_key);

  {
    auto storage = INvStorage::newStorage(config.storage);
    auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
    sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config1));
    sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config2));
    EXPECT_NO_THROW(sota_client->initialize());
    UptaneKey_Check_Test::checkKeyTests(storage, *sota_client);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
