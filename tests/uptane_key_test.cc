/**
 * \file
 */

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/polymorphic_pointer_cast.hpp>

#include "httpfake.h"
#include "logging/logging.h"
#include "primary/sotauptaneclient.h"
#include "storage/fsstorage.h"
#include "uptane/managedsecondary.h"
#include "uptane/secondaryconfig.h"
#include "uptane/uptanerepository.h"

void initKeyTests(Config& config, Uptane::SecondaryConfig& ecu_config1, Uptane::SecondaryConfig& ecu_config2,
                  TemporaryDirectory& temp_dir, const std::string& tls_server) {
  config.uptane.repo_server = tls_server + "/director";
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = kAutomatic;
  config.tls.server = tls_server;
  config.uptane.repo_server = tls_server + "/repo";
  config.uptane.primary_ecu_serial = "testecuserial";
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_public_key_path = "public.key";
  config.pacman.type = kNone;

  ecu_config1.secondary_type = Uptane::kVirtual;
  ecu_config1.partial_verifying = false;
  ecu_config1.full_client_dir = temp_dir.Path();
  ecu_config1.ecu_serial = "secondary_ecu_serial1";
  ecu_config1.ecu_hardware_id = "secondary_hardware1";
  ecu_config1.ecu_private_key = "sec1.priv";
  ecu_config1.ecu_public_key = "sec1.pub";
  ecu_config1.firmware_path = temp_dir / "firmware1.txt";
  ecu_config1.target_name_path = temp_dir / "firmware1_name.txt";
  ecu_config1.metadata_path = temp_dir / "secondary1_metadata";
  config.uptane.secondary_configs.push_back(ecu_config1);

  ecu_config2.secondary_type = Uptane::kVirtual;
  ecu_config2.partial_verifying = false;
  ecu_config2.full_client_dir = temp_dir.Path();
  ecu_config2.ecu_serial = "secondary_ecu_serial2";
  ecu_config2.ecu_hardware_id = "secondary_hardware2";
  ecu_config2.ecu_private_key = "sec2.priv";
  ecu_config2.ecu_public_key = "sec2.pub";
  ecu_config2.firmware_path = temp_dir / "firmware2.txt";
  ecu_config2.target_name_path = temp_dir / "firmware2_name.txt";
  ecu_config2.metadata_path = temp_dir / "secondary2_metadata";
  config.uptane.secondary_configs.push_back(ecu_config2);
}

void checkKeyTests(std::shared_ptr<INvStorage>& storage, SotaUptaneClient& sota_client) {
  std::string ca;
  std::string cert;
  std::string pkey;
  EXPECT_TRUE(storage->loadTlsCreds(&ca, &cert, &pkey));
  EXPECT_TRUE(ca.size() > 0);
  EXPECT_TRUE(cert.size() > 0);
  EXPECT_TRUE(pkey.size() > 0);

  std::string primary_public;
  std::string primary_private;
  EXPECT_TRUE(storage->loadPrimaryKeys(&primary_public, &primary_private));
  EXPECT_TRUE(primary_public.size() > 0);
  EXPECT_TRUE(primary_private.size() > 0);

  std::vector<std::pair<std::string, std::string> > ecu_serials;
  EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials));
  EXPECT_EQ(ecu_serials.size(), 3);

  std::vector<std::string> public_keys;
  std::vector<std::string> private_keys;
  std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >::iterator it;
  for (it = sota_client.secondaries.begin(); it != sota_client.secondaries.end(); it++) {
    if (it->second->sconfig.secondary_type != Uptane::kVirtual &&
        it->second->sconfig.secondary_type != Uptane::kLegacy) {
      continue;
    }
    std::shared_ptr<Uptane::ManagedSecondary> managed =
        boost::polymorphic_pointer_downcast<Uptane::ManagedSecondary>(it->second);
    std::string public_key;
    std::string private_key;
    EXPECT_TRUE(managed->loadKeys(&public_key, &private_key));
    EXPECT_TRUE(public_key.size() > 0);
    EXPECT_TRUE(private_key.size() > 0);
    EXPECT_NE(public_key, private_key);
    public_keys.push_back(public_key);
    private_keys.push_back(private_key);
  }
  std::sort(public_keys.begin(), public_keys.end());
  EXPECT_EQ(adjacent_find(public_keys.begin(), public_keys.end()), public_keys.end());
  std::sort(private_keys.begin(), private_keys.end());
  EXPECT_EQ(adjacent_find(private_keys.begin(), private_keys.end()), private_keys.end());
}

/**
 * \verify{\tst{159}} Check that all keys are present after successful
 * provisioning.
 */
TEST(UptaneKey, CheckAllKeys) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  Uptane::SecondaryConfig ecu_config1;
  Uptane::SecondaryConfig ecu_config2;
  initKeyTests(config, ecu_config1, ecu_config2, temp_dir, http.tls_server);

  auto storage = INvStorage::newStorage(config.storage);
  Uptane::Repository uptane(config, storage, http);
  std::shared_ptr<event::Channel> events_channel{new event::Channel};
  SotaUptaneClient sota_client(config, events_channel, uptane, storage, http);
  EXPECT_TRUE(uptane.initialize());
  checkKeyTests(storage, sota_client);
}

/**
 * \verify{\tst{160}} Check that aktualizr can recover from a half done device
 * registration.
 */
TEST(UptaneKey, RecoverWithoutKeys) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  Uptane::SecondaryConfig ecu_config1;
  Uptane::SecondaryConfig ecu_config2;
  initKeyTests(config, ecu_config1, ecu_config2, temp_dir, http.tls_server);

  {
    auto storage = INvStorage::newStorage(config.storage);
    Uptane::Repository uptane(config, storage, http);
    std::shared_ptr<event::Channel> events_channel{new event::Channel};
    SotaUptaneClient sota_client(config, events_channel, uptane, storage, http);

    EXPECT_TRUE(uptane.initialize());
    checkKeyTests(storage, sota_client);

    // Remove TLS keys but keep ECU keys and try to initialize.
    storage->clearTlsCreds();
  }
  {
    auto storage = INvStorage::newStorage(config.storage);
    Uptane::Repository uptane(config, storage, http);
    std::shared_ptr<event::Channel> events_channel{new event::Channel};
    SotaUptaneClient sota_client(config, events_channel, uptane, storage, http);

    EXPECT_TRUE(uptane.initialize());
    checkKeyTests(storage, sota_client);
  }

  // Remove ECU keys but keep TLS keys and try to initialize.
  boost::filesystem::remove(config.storage.path / config.storage.uptane_public_key_path);
  boost::filesystem::remove(config.storage.path / config.storage.uptane_private_key_path);
  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_public_key);
  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_private_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_public_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_private_key);

  {
    auto storage = INvStorage::newStorage(config.storage);
    Uptane::Repository uptane(config, storage, http);
    std::shared_ptr<event::Channel> events_channel{new event::Channel};
    SotaUptaneClient sota_client(config, events_channel, uptane, storage, http);

    EXPECT_TRUE(uptane.initialize());
    checkKeyTests(storage, sota_client);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
