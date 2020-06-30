#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "libaktualizr/config.h"

#include "bootstrap/bootstrap.h"
#include "crypto/crypto.h"
#include "test_utils.h"
#include "utilities/utils.h"

namespace bpo = boost::program_options;
boost::filesystem::path build_dir;

TEST(config, DefaultValues) {
  Config conf;
  EXPECT_EQ(conf.uptane.key_type, KeyType::kRSA2048);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);
}

TEST(config, TomlBasic) {
  Config conf("tests/config/basic.toml");
  EXPECT_EQ(conf.pacman.type, PACKAGE_MANAGER_NONE);
}

TEST(config, TomlEmpty) {
  Config conf;
  conf.updateFromTomlString("");
  EXPECT_EQ(conf.uptane.key_type, KeyType::kRSA2048);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);
}

TEST(config, TomlInt) {
  Config conf;
  conf.updateFromTomlString("[uptane]\nkey_type = \"ED25519\"\npolling_sec = 99\n");
  EXPECT_EQ(conf.uptane.key_type, KeyType::kED25519);
  EXPECT_EQ(conf.uptane.polling_sec, 99u);
}

/*
 * Check that user can specify Primary serial via a config file.
 */
TEST(config, TomlPrimarySerial) {
  RecordProperty("zephyr_key", "OTA-988");
  Config conf("tests/config/testupdate.toml");
  EXPECT_EQ(conf.provision.primary_ecu_serial, "723f79763eda1c753ce565c16862c79acdde32eb922d6662f088083c51ffde66");
}

/*
 * Check that user can specify Primary serial on the command line.
 */
TEST(config, CmdlPrimarySerial) {
  RecordProperty("zephyr_key", "OTA-988");
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

/*
 * Extract credentials from a provided archive.
 */
TEST(config, ExtractCredentials) {
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

/**
 * Start in device credential provisioning mode.
 */
TEST(config, DeviceCredMode) {
  RecordProperty("zephyr_key", "OTA-996,TST-184");
  Config config;
  EXPECT_EQ(config.provision.mode, ProvisionMode::kDeviceCred);
}

/**
 * Start in shared credential provisioning mode.
 */
TEST(config, SharedCredMode) {
  Config config("tests/config/basic.toml");
  EXPECT_EQ(config.provision.mode, ProvisionMode::kSharedCred);
}

/* Write config to file or to the log.
 * We don't normally dump the config to file anymore, but we do write it to the
 * log. */
TEST(config, TomlConsistentEmpty) {
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

TEST(config, TomlConsistentNonempty) {
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

static std::vector<boost::filesystem::path> generate_multi_config(TemporaryDirectory &temp_dir) {
  std::string content;
  {
    content += "[storage]\n";
    content += "path = \"path_a\"\n";
    content += "\n";
    content += "[pacman]\n";
    content += "os = \"os_a\"";
  }
  Utils::writeFile((temp_dir / "a_dir/a.toml"), content);

  content.clear();
  {
    content += "[storage]\n";
    content += "path = \"path_z\"\n";
    content += "\n";
    content += "[pacman]\n";
    content += "sysroot = \"sysroot_z\"";
  }
  Utils::writeFile((temp_dir / "a_dir/z.toml"), content);

  content.clear();
  {
    content += "[storage]\n";
    content += "path = \"cecond_a\"";
  }
  Utils::writeFile((temp_dir / "b_dir/a.toml"), content);

  content.clear();
  {
    content += "[storage]\n";
    content += "path = \"latest_path\"\n";
    content += "\n";
    content += "[provision]\n";
    content += "provision_path = \"y_prov_path\"";
  }
  Utils::writeFile((temp_dir / "b_dir/y.toml"), content);

  content.clear();
  {
    content += "[storage]\n";
    content += "path = \"this is path from text file\"";
  }
  Utils::writeFile((temp_dir / "b_dir/z.txt"), content);

  return std::vector<boost::filesystem::path>{(temp_dir / "a_dir"), (temp_dir / "b_dir")};
}
/* Parse multiple config files in a directory. */
TEST(config, OneDir) {
  TemporaryDirectory temp_dir;
  std::vector<boost::filesystem::path> dirs = generate_multi_config(temp_dir);

  Config config(std::vector<boost::filesystem::path>{dirs[0]});
  EXPECT_EQ(config.storage.path.string(), "path_z");
  EXPECT_EQ(config.pacman.sysroot.string(), "sysroot_z");
  EXPECT_EQ(config.pacman.os, "os_a");
}

/* Parse multiple config files in multiple directories. */
TEST(config, TwoDirs) {
  TemporaryDirectory temp_dir;
  std::vector<boost::filesystem::path> dirs = generate_multi_config(temp_dir);

  Config config(dirs);
  EXPECT_EQ(config.storage.path.string(), "path_z");
  EXPECT_EQ(config.pacman.sysroot.string(), "sysroot_z");
  EXPECT_NE(config.pacman.os, "os_a");
  EXPECT_EQ(config.provision.provision_path.string(), "y_prov_path");
}

void checkConfigExpectations(const Config &conf) {
  EXPECT_EQ(conf.storage.type, StorageType::kSqlite);
  EXPECT_EQ(conf.pacman.type, PACKAGE_MANAGER_NONE);
  EXPECT_EQ(conf.tls.ca_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.tls.pkey_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.tls.cert_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.uptane.key_source, CryptoSource::kPkcs11);
  EXPECT_EQ(conf.uptane.key_type, KeyType::kED25519);
  EXPECT_EQ(conf.bootloader.rollback_mode, RollbackMode::kUbootMasked);
}

/* This test is designed to catch a bug in which storage.type and pacman.type
 * set in the first config file read could be overwritten by the defaults when
 * reading a second config file. */
TEST(config, TwoTomlCorrectness) {
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
