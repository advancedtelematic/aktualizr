#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "bootstrap/bootstrap.h"
#include "config/config.h"
#include "crypto/crypto.h"
#include "test_utils.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;
boost::filesystem::path build_dir;

TEST(config, config_initialized_values) {
  Config conf;

  EXPECT_EQ(conf.uptane.running_mode, RunningMode::kFull);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);
}

TEST(config, config_toml_parsing) {
  Config conf("tests/config/basic.toml");

  EXPECT_EQ(conf.pacman.type, PackageManager::kNone);
}

TEST(config, config_toml_parsing_empty_file) {
  Config conf;
  conf.updateFromTomlString("");

  EXPECT_EQ(conf.uptane.running_mode, RunningMode::kFull);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);
}

TEST(config, config_toml_int) {
  Config conf;
  conf.updateFromTomlString("[uptane]\nrunning_mode = once\npolling_sec = 99\n");

  EXPECT_EQ(conf.uptane.running_mode, RunningMode::kOnce);
  EXPECT_EQ(conf.uptane.polling_sec, 99u);
}
TEST(config, config_toml_netport) {
  Config conf;
  conf.updateFromTomlString("[network]\nipuptane_port = 9099\n\n");
  EXPECT_EQ(conf.network.ipuptane_port, 9099);
}

TEST(config, config_cmdl_parsing) {
  constexpr int argc = 5;
  const char *argv[argc] = {"./aktualizr", "--primary-ecu-serial", "test-serial", "-c", "tests/config/minimal.toml"};

  bpo::options_description description("CommandLine Options");
  // clang-format off
  description.add_options()
    ("primary-ecu-serial", bpo::value<std::string>(), "serial number of primary ecu")
    ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration directory");
  // clang-format on

  bpo::variables_map vm;
  bpo::store(bpo::parse_command_line(argc, argv, description), vm);
  Config conf(vm);

  EXPECT_EQ(conf.provision.primary_ecu_serial, "test-serial");
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
    ("secondary-config", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "secondary ECU json configuration file")
    ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration directory");

  // clang-format on
  const char *argv[] = {"aktualizr", "--secondary-config", "config/secondary/virtualsec.json", "-c",
                        conf_path_str.c_str()};
  bpo::store(bpo::parse_command_line(5, argv, description), cmd);

  Config conf(cmd);
  EXPECT_EQ(conf.uptane.secondary_configs.size(), 1);
  EXPECT_EQ(conf.uptane.secondary_configs[0].secondary_type, Uptane::SecondaryType::kVirtual);
  EXPECT_EQ(conf.uptane.secondary_configs[0].ecu_hardware_id, "demo-virtual");
  // If not provided, serial is not generated until SotaUptaneClient is initialized.
  EXPECT_TRUE(conf.uptane.secondary_configs[0].ecu_serial.empty());
}

void checkSecondaryConfig(const Config &conf) {
  EXPECT_EQ(conf.uptane.secondary_configs.size(), 2);
  EXPECT_EQ(conf.uptane.secondary_configs[0].secondary_type, Uptane::SecondaryType::kLegacy);
  EXPECT_EQ(conf.uptane.secondary_configs[0].ecu_hardware_id, "example1");
  EXPECT_FALSE(conf.uptane.secondary_configs[0].ecu_serial.empty());
  EXPECT_EQ(conf.uptane.secondary_configs[1].secondary_type, Uptane::SecondaryType::kLegacy);
  EXPECT_EQ(conf.uptane.secondary_configs[1].ecu_hardware_id, "example2");
  // If not provided, serial is not generated until SotaUptaneClient is initialized.
  EXPECT_TRUE(conf.uptane.secondary_configs[1].ecu_serial.empty());
}

TEST(config, legacy_interface_cmdline) {
  TemporaryDirectory temp_dir;
  const std::string conf_path_str = (temp_dir.Path() / "config.toml").string();
  TestUtils::writePathToConfig("tests/config/minimal.toml", conf_path_str, temp_dir.Path());

  bpo::variables_map cmd;
  bpo::options_description description("some text");
  // clang-format off
  description.add_options()
    ("legacy-interface", bpo::value<boost::filesystem::path>()->composing(), "path to legacy secondary ECU interface program")
    ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration directory");

  // clang-format on
  std::string path = (build_dir / "src/external_secondaries/example-interface").string();
  const char *argv[] = {"aktualizr", "--legacy-interface", path.c_str(), "-c", conf_path_str.c_str()};
  bpo::store(bpo::parse_command_line(5, argv, description), cmd);

  Config conf(cmd);
  checkSecondaryConfig(conf);
}

TEST(config, legacy_interface_config) {
  Config conf("tests/config/minimal.toml");
  conf.uptane.legacy_interface = build_dir / "src/external_secondaries/example-interface";
  conf.postUpdateValues();
  checkSecondaryConfig(conf);
}

/**
 * Verify that aktualizr can start in implicit provisioning mode.
 */
TEST(config, implicit_mode) {
  RecordProperty("zephyr_key", "OTA-601,TST-184");
  Config config;
  EXPECT_EQ(config.provision.mode, ProvisionMode::kImplicit);
}

TEST(config, automatic_mode) {
  Config config("tests/config/basic.toml");
  EXPECT_EQ(config.provision.mode, ProvisionMode::kAutomatic);
}

/* We don't normally dump the config to file anymore, but we do write it to the
 * log. */
TEST(config, consistent_toml_empty) {
  TemporaryDirectory temp_dir;
  Config config1;
  std::ofstream sink1((temp_dir / "output1.toml").c_str(), std::ofstream::out);
  config1.writeToStream(sink1);

  Config config2((temp_dir / "output1.toml").string());
  std::ofstream sink2((temp_dir / "output2.toml").c_str(), std::ofstream::out);
  config2.writeToStream(sink2);

  std::string conf_str1 = Utils::readFile((temp_dir / "output1.toml").string());
  std::string conf_str2 = Utils::readFile((temp_dir / "output2.toml").string());
  EXPECT_EQ(conf_str1, conf_str2);
}

TEST(config, consistent_toml_nonempty) {
  TemporaryDirectory temp_dir;
  Config config1("tests/config/basic.toml");
  std::ofstream sink1((temp_dir / "output1.toml").c_str(), std::ofstream::out);
  config1.writeToStream(sink1);

  Config config2((temp_dir / "output1.toml").string());
  std::ofstream sink2((temp_dir / "output2.toml").c_str(), std::ofstream::out);
  config2.writeToStream(sink2);

  std::string conf_str1 = Utils::readFile((temp_dir / "output1.toml").string());
  std::string conf_str2 = Utils::readFile((temp_dir / "output2.toml").string());
  EXPECT_EQ(conf_str1, conf_str2);
}

TEST(config, config_dirs_one_dir) {
  Config config(std::vector<boost::filesystem::path>{"tests/test_data/config_dirs/one_dir"});
  EXPECT_EQ(config.storage.path.string(), "path_z");
  EXPECT_EQ(config.pacman.sysroot.string(), "sysroot_z");
  EXPECT_EQ(config.pacman.os, "os_a");
}

TEST(config, config_dirs_two_dirs) {
  std::vector<boost::filesystem::path> config_dirs{"tests/test_data/config_dirs/one_dir",
                                                   "tests/test_data/config_dirs/second_one_dir"};
  Config config(config_dirs);
  EXPECT_EQ(config.storage.path.string(), "path_z");
  EXPECT_EQ(config.pacman.sysroot.string(), "sysroot_z");
  EXPECT_NE(config.pacman.os, "os_a");
  EXPECT_EQ(config.provision.provision_path.string(), "y_prov_path");
}

void checkConfigExpectations(const Config &conf) {
  EXPECT_EQ(conf.storage.type, StorageType::kSqlite);
  EXPECT_EQ(conf.pacman.type, PackageManager::kNone);
  EXPECT_EQ(conf.tls.ca_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.tls.pkey_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.tls.cert_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.uptane.running_mode, RunningMode::kCheck);
  EXPECT_EQ(conf.uptane.key_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.uptane.key_type, KeyType::kED25519);
  EXPECT_EQ(conf.bootloader.rollback_mode, RollbackMode::kUbootMasked);
}

/* This test is designed to catch a bug in which storage.type and pacman.type
 * set in the first config file read could be overwritten by the defaults when
 * reading a second config file. */
TEST(config, two_config_correctness) {
  TemporaryDirectory temp_dir;
  const std::string conf_path_str = (temp_dir.Path() / "config.toml").string();
  TestUtils::writePathToConfig("tests/config/minimal.toml", conf_path_str, temp_dir.Path());
  {
    std::ofstream cs(conf_path_str.c_str(), std::ofstream::app);
    cs << "type = \"sqlite\"\n";
    cs << "\n";
    cs << "[pacman]\n";
    cs << "type = \"none\"\n";
    cs << "\n";
    cs << "[tls]\n";
    cs << "ca_source = \"pkcs11\"\n";
    cs << "pkey_source = \"pkcs11\"\n";
    cs << "cert_source = \"pkcs11\"\n";
    cs << "\n";
    cs << "[uptane]\n";
    cs << "running_mode = \"check\"\n";
    cs << "key_source = \"pkcs11\"\n";
    cs << "key_type = \"ED25519\"\n";
    cs << "\n";
    cs << "[bootloader]\n";
    cs << "rollback_mode = \"uboot_masked\"\n";
  }

  bpo::variables_map cmd;
  bpo::options_description description("some text");
  // clang-format off
  description.add_options()
    ("config,c", bpo::value<std::vector<boost::filesystem::path> >()->composing(), "configuration directory");
  // clang-format on

  const char *argv1[] = {"aktualizr", "-c", conf_path_str.c_str(), "-c", "tests/config/minimal.toml"};
  bpo::store(bpo::parse_command_line(5, argv1, description), cmd);
  Config conf1(cmd);
  checkConfigExpectations(conf1);

  // Try the reverse order, too, just to make sure.
  const char *argv2[] = {"aktualizr", "-c", "tests/config/minimal.toml", "-c", conf_path_str.c_str()};
  bpo::store(bpo::parse_command_line(5, argv2, description), cmd);
  Config conf2(cmd);
  checkConfigExpectations(conf2);
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
