/**
 * \file
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "httpfake.h"
#include "logging/logging.h"
#include "primary/sotauptaneclient.h"
#include "storage/fsstorage.h"
#include "test_utils.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;

boost::filesystem::path build_dir;

/**
 * \verify{\tst{155}} Check that aktualizr generates random ecu_serial for
 * primary and all secondaries.
 */
TEST(Uptane, RandomSerial) {
  TemporaryDirectory temp_dir1, temp_dir2;
  Config conf_1("tests/config/basic.toml");
  conf_1.storage.path = temp_dir1.Path();
  Config conf_2("tests/config/basic.toml");
  conf_2.storage.path = temp_dir2.Path();

  conf_1.uptane.primary_ecu_serial = "";
  conf_1.storage.uptane_private_key_path = "private.key";
  conf_1.storage.uptane_public_key_path = "public.key";

  conf_2.uptane.primary_ecu_serial = "";
  conf_2.storage.uptane_private_key_path = "private.key";
  conf_2.storage.uptane_public_key_path = "public.key";

  // add secondaries
  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
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
  HttpFake http1(temp_dir1.Path());
  HttpFake http2(temp_dir2.Path());

  Uptane::Repository uptane_1(conf_1, storage_1, http1);
  SotaUptaneClient uptane_client1(conf_1, NULL, uptane_1, storage_1, http1);
  EXPECT_TRUE(uptane_1.initialize());

  Uptane::Repository uptane_2(conf_2, storage_2, http1);
  SotaUptaneClient uptane_client2(conf_2, NULL, uptane_2, storage_2, http2);
  EXPECT_TRUE(uptane_2.initialize());

  std::vector<std::pair<std::string, std::string> > ecu_serials_1;
  std::vector<std::pair<std::string, std::string> > ecu_serials_2;

  EXPECT_TRUE(storage_1->loadEcuSerials(&ecu_serials_1));
  EXPECT_TRUE(storage_2->loadEcuSerials(&ecu_serials_2));
  EXPECT_EQ(ecu_serials_1.size(), 2);
  EXPECT_EQ(ecu_serials_2.size(), 2);
  EXPECT_FALSE(ecu_serials_1[0].first.empty());
  EXPECT_FALSE(ecu_serials_1[1].first.empty());
  EXPECT_FALSE(ecu_serials_2[0].first.empty());
  EXPECT_FALSE(ecu_serials_2[1].first.empty());
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_NE(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);
}

/**
 * \verify{\tst{156}} Check that aktualizr saves random ecu_serial for primary
 * and all secondaries.
 */
TEST(Uptane, ReloadSerial) {
  TemporaryDirectory temp_dir;
  std::vector<std::pair<std::string, std::string> > ecu_serials_1;
  std::vector<std::pair<std::string, std::string> > ecu_serials_2;

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
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
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";
    conf.uptane.secondary_configs.push_back(ecu_config);

    auto storage = INvStorage::newStorage(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane, storage, http);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_1));
    EXPECT_EQ(ecu_serials_1.size(), 2);
    EXPECT_FALSE(ecu_serials_1[0].first.empty());
    EXPECT_FALSE(ecu_serials_1[1].first.empty());
  }

  // Initialize new objects and load serials.
  {
    Config conf("tests/config/basic.toml");
    conf.storage.path = temp_dir.Path();
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";
    conf.uptane.secondary_configs.push_back(ecu_config);

    auto storage = INvStorage::newStorage(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane, storage, http);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_2));
    EXPECT_EQ(ecu_serials_2.size(), 2);
    EXPECT_FALSE(ecu_serials_2[0].first.empty());
    EXPECT_FALSE(ecu_serials_2[1].first.empty());
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
    ("legacy-interface", bpo::value<boost::filesystem::path>()->composing(), "path to legacy secondary ECU interface program");
  // clang-format on
  std::string path = (build_dir / "src/external_secondaries/example-interface").string();
  const char* argv[] = {"aktualizr", "--legacy-interface", path.c_str()};
  bpo::store(bpo::parse_command_line(3, argv, description), cmd);

  std::vector<std::pair<std::string, std::string> > ecu_serials_1;
  std::vector<std::pair<std::string, std::string> > ecu_serials_2;

  // Initialize and store serials.
  {
    Config conf(conf_path_str, cmd);
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";

    auto storage = INvStorage::newStorage(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane, storage, http);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_1));
    EXPECT_EQ(ecu_serials_1.size(), 3);
    EXPECT_FALSE(ecu_serials_1[0].first.empty());
    EXPECT_FALSE(ecu_serials_1[1].first.empty());
    EXPECT_FALSE(ecu_serials_1[2].first.empty());
  }

  // Initialize new objects and load serials.
  {
    Config conf(conf_path_str, cmd);
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";

    auto storage = INvStorage::newStorage(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane, storage, http);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage->loadEcuSerials(&ecu_serials_2));
    EXPECT_EQ(ecu_serials_2.size(), 3);
    EXPECT_FALSE(ecu_serials_2[0].first.empty());
    EXPECT_FALSE(ecu_serials_2[1].first.empty());
    EXPECT_FALSE(ecu_serials_2[2].first.empty());
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
