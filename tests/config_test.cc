/**
 * \file
 */

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "bootstrap.h"

#include "test_utils.h"
#include "utilities/crypto.h"
#include "utilities/utils.h"

#include "config.h"

namespace bpo = boost::program_options;
boost::filesystem::path build_dir;

TEST(config, config_initialized_values) {
  Config conf;

  EXPECT_EQ(conf.uptane.polling, true);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);
}

TEST(config, config_toml_parsing) {
  Config conf("tests/config/minimal.toml");

  EXPECT_EQ(conf.gateway.socket, true);
}

TEST(config, config_toml_parsing_empty_file) {
  Config conf;
  conf.updateFromTomlString("");

  EXPECT_EQ(conf.uptane.polling, true);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);
}

TEST(config, config_cmdl_parsing) {
  constexpr int argc = 3;
  const char *argv[argc] = {"./aktualizr", "--gateway-socket", "on"};

  bpo::options_description description("CommandLine Options");
  // clang-format off
  description.add_options()
    ("gateway-socket", bpo::value<bool>(), "on/off the socket gateway");
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::parse_command_line(argc, argv, description), vm);
  Config conf("tests/config/minimal.toml", vm);

  EXPECT_TRUE(conf.gateway.socket);
}

TEST(config, config_extract_credentials) {
  TemporaryDirectory temp_dir;
  Config conf;
  conf.storage.path = temp_dir.Path();
  conf.provision.provision_path = "tests/test_data/credentials.zip";
  conf.tls.server.clear();
  conf.postUpdateValues();
  EXPECT_EQ(conf.tls.server, "https://bd8012b4-cf0f-46ca-9d2c-46a41d534af5.tcpgw.prod01.advancedtelematic.com:443");

  Bootstrap boot(conf.provision.provision_path, "");
  EXPECT_EQ(boost::algorithm::hex(Crypto::sha256digest(boot.getCa())),
            "FBA3C8FAD16D8B3EC64F7D47CBDD8456A51A6399734A3F6B7E2D6E562072F264");
  std::cout << "Certificate: " << boot.getCert() << std::endl;
  EXPECT_EQ(boost::algorithm::hex(Crypto::sha256digest(boot.getCert())),
            "02300CC9797556915D88CFA05644BFF22D8C458367A3636F7921585F828ECB81");
  std::cout << "Pkey: " << boot.getPkey() << std::endl;
  EXPECT_EQ(boost::algorithm::hex(Crypto::sha256digest(boot.getPkey())),
            "D27E3E56BEF02AAA6D6FFEFDA5357458C477A8E891C5EADF4F04CE67BB5866A4");
}

TEST(config, secondary_config) {
  TemporaryDirectory temp_dir;
  const std::string conf_path_str = (temp_dir.Path() / "config.toml").string();
  TestUtils::writePathToConfig("tests/config/minimal.toml", conf_path_str, temp_dir.Path());

  bpo::variables_map cmd;
  bpo::options_description description("some text");
  // clang-format off
  description.add_options()
    ("secondary-config", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "secondary ECU json configuration file");
  // clang-format on
  const char *argv[] = {"aktualizr", "--secondary-config", "config/secondary/virtualsec.json"};
  bpo::store(bpo::parse_command_line(3, argv, description), cmd);

  Config conf(conf_path_str, cmd);
  EXPECT_EQ(conf.uptane.secondary_configs.size(), 1);
  EXPECT_EQ(conf.uptane.secondary_configs[0].secondary_type, Uptane::kVirtual);
  EXPECT_EQ(conf.uptane.secondary_configs[0].ecu_hardware_id, "demo-virtual");
  // If not provided, serial is not generated until SotaUptaneClient is initialized.
  EXPECT_TRUE(conf.uptane.secondary_configs[0].ecu_serial.empty());
}

TEST(config, legacy_interface) {
  TemporaryDirectory temp_dir;
  const std::string conf_path_str = (temp_dir.Path() / "config.toml").string();
  TestUtils::writePathToConfig("tests/config/minimal.toml", conf_path_str, temp_dir.Path());

  bpo::variables_map cmd;
  bpo::options_description description("some text");
  // clang-format off
  description.add_options()
    ("legacy-interface", bpo::value<boost::filesystem::path>()->composing(), "path to legacy secondary ECU interface program");
  // clang-format on
  std::string path = (build_dir / "src/external_secondaries/example-interface").string();
  const char *argv[] = {"aktualizr", "--legacy-interface", path.c_str()};
  bpo::store(bpo::parse_command_line(3, argv, description), cmd);

  Config conf(conf_path_str, cmd);
  EXPECT_EQ(conf.uptane.secondary_configs.size(), 2);
  EXPECT_EQ(conf.uptane.secondary_configs[0].secondary_type, Uptane::kLegacy);
  EXPECT_EQ(conf.uptane.secondary_configs[0].ecu_hardware_id, "example1");
  EXPECT_FALSE(conf.uptane.secondary_configs[0].ecu_serial.empty());
  EXPECT_EQ(conf.uptane.secondary_configs[1].secondary_type, Uptane::kLegacy);
  EXPECT_EQ(conf.uptane.secondary_configs[1].ecu_hardware_id, "example2");
  // If not provided, serial is not generated until SotaUptaneClient is initialized.
  EXPECT_TRUE(conf.uptane.secondary_configs[1].ecu_serial.empty());
}

/**
 * \verify{\tst{184}} Verify that aktualizr can start in implicit provisioning
 * mode.
 */
TEST(config, implicit_mode) {
  Config config;
  EXPECT_EQ(config.provision.mode, kImplicit);
}

TEST(config, automatic_mode) {
  Config config("tests/config/basic.toml");
  EXPECT_EQ(config.provision.mode, kAutomatic);
}

TEST(config, consistent_toml_empty) {
  TemporaryDirectory temp_dir;
  Config config1;
  config1.writeToFile((temp_dir / "output1.toml").string());
  Config config2((temp_dir / "output1.toml").string());
  config2.writeToFile((temp_dir / "output2.toml").string());
  std::string conf_str1 = Utils::readFile((temp_dir / "output1.toml").string());
  std::string conf_str2 = Utils::readFile((temp_dir / "output2.toml").string());
  EXPECT_EQ(conf_str1, conf_str2);
}

TEST(config, consistent_toml_nonempty) {
  TemporaryDirectory temp_dir;
  Config config1("tests/config/basic.toml");
  config1.writeToFile((temp_dir / "output1.toml").string());
  Config config2((temp_dir / "output1.toml").string());
  config2.writeToFile((temp_dir / "output2.toml").string());
  std::string conf_str1 = Utils::readFile((temp_dir / "output1.toml").string());
  std::string conf_str2 = Utils::readFile((temp_dir / "output2.toml").string());
  EXPECT_EQ(conf_str1, conf_str2);
}

TEST(config, config_dirs_one_dir) {
  Config config(std::vector<boost::filesystem::path>{"tests/test_data/config_dirs/one_dir"});
  EXPECT_EQ(config.storage.path, "/z_path");
}

TEST(config, config_dirs_two_dirs) {
  std::vector<boost::filesystem::path> config_dirs{"tests/test_data/config_dirs/one_dir",
                                                   "tests/test_data/config_dirs/second_one_dir"};
  Config config(config_dirs);
  EXPECT_EQ(config.storage.path, "/latest_path");
}

TEST(config, invalid_loglevel) {
  Config config;
  config.logger.loglevel = static_cast<boost::log::trivial::severity_level>(6);
  config.postUpdateValues();
  EXPECT_EQ(config.logger.loglevel, boost::log::trivial::fatal);
}

TEST(config, negative_loglevel) {
  Config config;
  config.logger.loglevel = static_cast<boost::log::trivial::severity_level>(-1);
  config.postUpdateValues();
  EXPECT_EQ(config.logger.loglevel, boost::log::trivial::trace);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the build directory as an input argument.\n";
    return EXIT_FAILURE;
  }
  build_dir = argv[1];
  return RUN_ALL_TESTS();
}
#endif
