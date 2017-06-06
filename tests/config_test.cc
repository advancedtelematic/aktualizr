#include <gtest/gtest.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include "config.h"

namespace bpo = boost::program_options;
extern bpo::variables_map parse_options(int argc, char *argv[]);

TEST(config, config_initialized_values) {
  Config conf;

  EXPECT_EQ(conf.core.server, "http://127.0.0.1:8080");
  EXPECT_EQ(conf.core.polling, true);
  EXPECT_EQ(conf.core.polling_sec, 10u);

  EXPECT_EQ(conf.auth.server, "http://127.0.0.1:9001");

  EXPECT_EQ(conf.device.uuid, "123e4567-e89b-12d3-a456-426655440000");
  EXPECT_EQ(conf.device.packages_dir, "/tmp/");

  EXPECT_EQ(conf.gateway.http, true);
  EXPECT_EQ(conf.gateway.rvi, false);
}

TEST(config, config_toml_parsing) {
  Config conf("tests/config_tests.toml");

  EXPECT_EQ(conf.core.server, "https://example.com/core");
  EXPECT_EQ(conf.core.polling, false);
  EXPECT_EQ(conf.core.polling_sec, 91u);

  EXPECT_EQ(conf.auth.server, "https://example.com/auth");
  EXPECT_EQ(conf.auth.client_id, "thisisaclientid");
  EXPECT_EQ(conf.auth.client_secret, "thisisaclientsecret");

  EXPECT_EQ(conf.device.uuid, "bc50fa11-eb93-41c0-b0fa-5ce56affa63e");
  EXPECT_EQ(conf.device.packages_dir, "/tmp/packages_dir");

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

TEST(config, config_oauth_tls_parsing) {
  try {
    Config conf("tests/config_tests_prov_bad.toml");
  } catch (std::logic_error e) {
    EXPECT_STREQ(e.what(),
                 "It is not possible to set [tls] section with 'auth.client_id' or 'auth.client_secret' properties");
  }
}

TEST(config, config_toml_parsing_empty_file) {
  Config conf;
  conf.updateFromTomlString("");

  EXPECT_EQ(conf.core.server, "http://127.0.0.1:8080");
  EXPECT_EQ(conf.core.polling, true);
  EXPECT_EQ(conf.core.polling_sec, 10u);

  EXPECT_EQ(conf.auth.server, "http://127.0.0.1:9001");

  EXPECT_EQ(conf.device.uuid, "123e4567-e89b-12d3-a456-426655440000");
  EXPECT_EQ(conf.device.packages_dir, "/tmp/");

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

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
