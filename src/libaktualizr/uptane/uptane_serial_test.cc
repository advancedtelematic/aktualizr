#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "httpfake.h"
#include "logging/logging.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "test_utils.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;

boost::filesystem::path build_dir;

/**
 * Check that aktualizr generates random ecu_serial for primary and all
 * secondaries.
 */
TEST(Uptane, RandomSerial) {
  RecordProperty("zephyr_key", "OTA-989,TST-155");
  TemporaryDirectory temp_dir1, temp_dir2;
  Config conf_1("tests/config/basic.toml");
  conf_1.storage.path = temp_dir1.Path();
  Config conf_2("tests/config/basic.toml");
  conf_2.storage.path = temp_dir2.Path();

  conf_1.provision.primary_ecu_serial = "";
  conf_2.provision.primary_ecu_serial = "";

  // add secondaries
  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::SecondaryType::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir1.Path() / "sec_1";
  ecu_config.ecu_serial = "";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir1.Path() / "firmware.txt";
  ecu_config.target_name_path = temp_dir1.Path() / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir1.Path() / "secondary_metadata";
  conf_1.uptane.secondary_configs.push_back(ecu_config);
  ecu_config.full_client_dir = temp_dir2.Path() / "sec_2";
  ecu_config.firmware_path = temp_dir2.Path() / "firmware.txt";
  ecu_config.target_name_path = temp_dir2.Path() / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir2.Path() / "secondary_metadata";
  conf_2.uptane.secondary_configs.push_back(ecu_config);

  auto storage_1 = INvStorage::newStorage(conf_1.storage);
  auto storage_2 = INvStorage::newStorage(conf_2.storage);
  auto http1 = std::make_shared<HttpFake>(temp_dir1.Path());
  auto http2 = std::make_shared<HttpFake>(temp_dir2.Path());

  auto uptane_client1 = SotaUptaneClient::newTestClient(conf_1, storage_1, http1);
  EXPECT_NO_THROW(uptane_client1->initialize());

  auto uptane_client2 = SotaUptaneClient::newTestClient(conf_2, storage_2, http2);
  EXPECT_NO_THROW(uptane_client2->initialize());

  EcuSerials ecu_serials_1;
  EcuSerials ecu_serials_2;

  EXPECT_TRUE(storage_1->loadEcuSerials(&ecu_serials_1));
  EXPECT_TRUE(storage_2->loadEcuSerials(&ecu_serials_2));
  EXPECT_EQ(ecu_serials_1.size(), 2);
  EXPECT_EQ(ecu_serials_2.size(), 2);
  EXPECT_FALSE(ecu_serials_1[0].first.ToString().empty());
  EXPECT_FALSE(ecu_serials_1[1].first.ToString().empty());
  EXPECT_FALSE(ecu_serials_2[0].first.ToString().empty());
  EXPECT_FALSE(ecu_serials_2[1].first.ToString().empty());
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_NE(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);
}

/**
 * Check that aktualizr saves random ecu_serial for primary and all secondaries.
 */
TEST(Uptane, ReloadSerial) {
  RecordProperty("zephyr_key", "OTA-989,TST-156");
  TemporaryDirectory temp_dir;
  EcuSerials ecu_serials_1;
  EcuSerials ecu_serials_2;

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::SecondaryType::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path() / "sec";
  ecu_config.ecu_serial = "";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir.Path() / "firmware.txt";
  ecu_config.target_name_path = temp_dir.Path() / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir.Path() / "secondary_metadata";

  // Initialize and store serials.
  {
    Config conf("tests/config/basic.toml");
    conf.storage.path = temp_dir.Path();
    conf.provision.primary_ecu_serial = "";
    conf.uptane.secondary_configs.push_back(ecu_config);

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    auto uptane_client = SotaUptaneClient::newTestClient(conf, storage, http);
    EXPECT_NO_THROW(uptane_client->initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_1));
    EXPECT_EQ(ecu_serials_1.size(), 2);
    EXPECT_FALSE(ecu_serials_1[0].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_1[1].first.ToString().empty());
  }

  // Initialize new objects and load serials.
  {
    Config conf("tests/config/basic.toml");
    conf.storage.path = temp_dir.Path();
    conf.provision.primary_ecu_serial = "";
    conf.uptane.secondary_configs.push_back(ecu_config);

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    auto uptane_client = SotaUptaneClient::newTestClient(conf, storage, http);
    EXPECT_NO_THROW(uptane_client->initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_2));
    EXPECT_EQ(ecu_serials_2.size(), 2);
    EXPECT_FALSE(ecu_serials_2[0].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_2[1].first.ToString().empty());
  }

  EXPECT_EQ(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_EQ(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);
}

TEST(Uptane, LegacySerial) {
  TemporaryDirectory temp_dir;
  const std::string conf_path_str = (temp_dir.Path() / "config.toml").string();
  TestUtils::writePathToConfig("tests/config/basic.toml", conf_path_str, temp_dir.Path());

  bpo::variables_map cmd;
  bpo::options_description description("some text");
  // clang-format off
  description.add_options()
    ("legacy-interface", bpo::value<boost::filesystem::path>()->composing(), "path to legacy secondary ECU interface program")
    ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration directory");

  // clang-format on
  std::string path = (build_dir / "src/external_secondaries/example-interface").string();
  const char* argv[] = {"aktualizr", "--legacy-interface", path.c_str(), "-c", conf_path_str.c_str()};
  bpo::store(bpo::parse_command_line(5, argv, description), cmd);

  EcuSerials ecu_serials_1;
  EcuSerials ecu_serials_2;

  // Initialize and store serials.
  {
    Config conf(cmd);
    conf.provision.primary_ecu_serial = "";

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    auto uptane_client = SotaUptaneClient::newTestClient(conf, storage, http);
    EXPECT_NO_THROW(uptane_client->initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_1));
    EXPECT_EQ(ecu_serials_1.size(), 3);
    EXPECT_FALSE(ecu_serials_1[0].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_1[1].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_1[2].first.ToString().empty());
  }

  // Initialize new objects and load serials.
  {
    Config conf(cmd);
    conf.provision.primary_ecu_serial = "";

    auto storage = INvStorage::newStorage(conf.storage);
    auto http = std::make_shared<HttpFake>(temp_dir.Path());
    auto uptane_client = SotaUptaneClient::newTestClient(conf, storage, http);
    EXPECT_NO_THROW(uptane_client->initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_2));
    EXPECT_EQ(ecu_serials_2.size(), 3);
    EXPECT_FALSE(ecu_serials_2[0].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_2[1].first.ToString().empty());
    EXPECT_FALSE(ecu_serials_2[2].first.ToString().empty());
  }

  EXPECT_EQ(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_EQ(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_EQ(ecu_serials_1[2].first, ecu_serials_2[2].first);

  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[2].first);
  EXPECT_NE(ecu_serials_1[1].first, ecu_serials_1[2].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[2].first);
  EXPECT_NE(ecu_serials_2[1].first, ecu_serials_2[2].first);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the build directory as an input argument.\n";
    return EXIT_FAILURE;
  }
  build_dir = argv[1];
  return RUN_ALL_TESTS();
}
#endif
