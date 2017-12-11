#include <gtest/gtest.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "bootstrap.h"
#include "config.h"
#include "crypto.h"
#include "utils.h"

namespace bpo = boost::program_options;
extern bpo::variables_map parse_options(int argc, char *argv[]);
const std::string config_test_dir = "tests/test_config";

TEST(config, config_initialized_values) {
  Config conf;

  EXPECT_EQ(conf.uptane.polling, true);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);

  EXPECT_EQ(conf.rvi.uuid, "123e4567-e89b-12d3-a456-426655440000");
  EXPECT_EQ(conf.rvi.packages_dir, "/tmp/");

  EXPECT_EQ(conf.gateway.http, true);
  EXPECT_EQ(conf.gateway.rvi, false);
}

TEST(config, config_toml_parsing) {
  Config conf("tests/config_tests.toml");

  EXPECT_EQ(conf.rvi.uuid, "bc50fa11-eb93-41c0-b0fa-5ce56affa63e");
  EXPECT_EQ(conf.rvi.packages_dir, "/tmp/packages_dir");

  EXPECT_EQ(conf.gateway.dbus, true);
  EXPECT_EQ(conf.gateway.http, false);
  EXPECT_EQ(conf.gateway.rvi, true);
  EXPECT_EQ(conf.gateway.socket, true);

  EXPECT_EQ(conf.rvi.node_host, "rvi.example.com");

  EXPECT_EQ(conf.rvi.node_port, "9999");
}

#ifdef WITH_GENIVI

TEST(config, config_toml_dbus_session) {
  Config conf;
  conf.dbus.bus = DBUS_BUS_SYSTEM;
  conf.updateFromTomlString("[dbus]\nbus = \"session\"");

  EXPECT_EQ(conf.dbus.bus, DBUS_BUS_SESSION);
}

TEST(config, config_toml_dbus_system) {
  Config conf;
  conf.dbus.bus = DBUS_BUS_SESSION;
  conf.updateFromTomlString("[dbus]\nbus = \"system\"");

  EXPECT_EQ(conf.dbus.bus, DBUS_BUS_SYSTEM);
}

TEST(config, config_toml_dbus_invalid) {
  Config conf;
  conf.dbus.bus = DBUS_BUS_SYSTEM;
  conf.updateFromTomlString("[dbus]\nbus = \"foo\"");

  EXPECT_EQ(conf.dbus.bus, DBUS_BUS_SYSTEM);

  conf.dbus.bus = DBUS_BUS_SESSION;
  conf.updateFromTomlString("[dbus]\nbus = 123");

  EXPECT_EQ(conf.dbus.bus, DBUS_BUS_SESSION);
}

#endif

TEST(config, config_toml_parsing_empty_file) {
  Config conf;
  conf.updateFromTomlString("");

  EXPECT_EQ(conf.uptane.polling, true);
  EXPECT_EQ(conf.uptane.polling_sec, 10u);

  EXPECT_EQ(conf.rvi.uuid, "123e4567-e89b-12d3-a456-426655440000");
  EXPECT_EQ(conf.rvi.packages_dir, "/tmp/");

  EXPECT_EQ(conf.gateway.http, true);
  EXPECT_EQ(conf.gateway.rvi, false);
}

TEST(config, config_cmdl_parsing) {
  int argc = 7;
  const char *argv[] = {"./aktualizr", "--gateway-http", "off", "--gateway-rvi", "on", "--gateway-socket", "on"};

  bpo::options_description description("CommandLine Options");
  description.add_options()("gateway-http", bpo::value<bool>(), "on/off the http gateway")(
      "gateway-rvi", bpo::value<bool>(), "on/off the rvi gateway")("gateway-socket", bpo::value<bool>(),
                                                                   "on/off the socket gateway");

  bpo::variables_map vm;
  bpo::store(bpo::parse_command_line(argc, argv, description), vm);
  Config conf("tests/config_tests.toml", vm);

  EXPECT_EQ(conf.gateway.http, false);
  EXPECT_EQ(conf.gateway.rvi, true);
  EXPECT_EQ(conf.gateway.socket, true);
}

TEST(config, config_extract_credentials) {
  Config conf;
  conf.storage.path = config_test_dir + "/prov";
  conf.provision.provision_path = "tests/test_data/credentials.zip";

  boost::filesystem::remove_all(config_test_dir + "/prov");
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

  boost::filesystem::remove_all(config_test_dir);
}

/**
 * \verify{\tst{156}} Check that aktualizr saves random ecu_serial for primary
 * and all secondaries.
 */
TEST(config, ecu_persist) {
  boost::program_options::variables_map cmd;
  boost::program_options::options_description description("some text");
  description.add_options()("secondary-config", bpo::value<std::vector<std::string> >()->composing(),
                            "set config for secondary");
  const char *argv[] = {"aktualizr", "--secondary-config", "config/ex/secondary/virtualsec.json"};
  boost::program_options::store(boost::program_options::parse_command_line(3, argv, description), cmd);

  boost::filesystem::remove_all(config_test_dir);
  Config conf1("tests/config_tests.toml", cmd);

  Config conf2("tests/config_tests.toml", cmd);
  EXPECT_EQ(conf1.uptane.primary_ecu_serial, conf2.uptane.primary_ecu_serial);
  EXPECT_EQ(conf1.uptane.secondary_configs.size(), 1);
  EXPECT_EQ(conf2.uptane.secondary_configs.size(), 1);

  boost::filesystem::remove_all(config_test_dir);
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
  Config config("tests/config_tests_prov.toml");
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
  Config config1("tests/config_tests_prov.toml");
  config1.writeToFile((temp_dir / "output1.toml").string());
  Config config2((temp_dir / "output1.toml").string());
  config2.writeToFile((temp_dir / "output2.toml").string());
  std::string conf_str1 = Utils::readFile((temp_dir / "output1.toml").string());
  std::string conf_str2 = Utils::readFile((temp_dir / "output2.toml").string());
  EXPECT_EQ(conf_str1, conf_str2);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
