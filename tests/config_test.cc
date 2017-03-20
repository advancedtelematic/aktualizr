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
  EXPECT_EQ(conf.core.polling_sec, 10);

  EXPECT_EQ(conf.auth.server, "http://127.0.0.1:9001");
  EXPECT_EQ(conf.auth.client_id, "client-id");
  EXPECT_EQ(conf.auth.client_secret, "client-secret");

  EXPECT_EQ(conf.device.uuid, "123e4567-e89b-12d3-a456-426655440000");
  EXPECT_EQ(conf.device.packages_dir, "/tmp/");

  EXPECT_EQ(conf.gateway.http, true);
  EXPECT_EQ(conf.gateway.rvi, false);
}

TEST(config, config_toml_parsing) {
  Config conf;
  conf.updateFromToml("tests/config_tests.toml");

  EXPECT_EQ(conf.core.server, "https://example.com/core");
  EXPECT_EQ(conf.core.polling, false);
  EXPECT_EQ(conf.core.polling_sec, 91);

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
  EXPECT_EQ(conf.rvi.client_config, "my/rvi_conf.json");
}

TEST(config, config_oauth_tls_parsing) {
  Config conf;
  try{
    conf.updateFromToml("tests/config_tests_prov_bad.toml");
  }catch(std::logic_error e){
    EXPECT_STREQ(e.what(), "It is not possible to set [tls] section with 'auth.client_id' or 'auth.client_secret' proprties");
  }
}

TEST(config, config_toml_parsing_empty_file) {
  Config conf;
  conf.updateFromToml("Testing/config.toml");

  EXPECT_EQ(conf.core.server, "http://127.0.0.1:8080");
  EXPECT_EQ(conf.core.polling, true);
  EXPECT_EQ(conf.core.polling_sec, 10);

  EXPECT_EQ(conf.auth.server, "http://127.0.0.1:9001");
  EXPECT_EQ(conf.auth.client_id, "client-id");
  EXPECT_EQ(conf.auth.client_secret, "client-secret");

  EXPECT_EQ(conf.device.uuid, "123e4567-e89b-12d3-a456-426655440000");
  EXPECT_EQ(conf.device.packages_dir, "/tmp/");

  EXPECT_EQ(conf.gateway.http, true);
  EXPECT_EQ(conf.gateway.rvi, false);
}

TEST(config, config_cmdl_parsing) {
  Config conf;
  int argc = 7;
  const char *argv[] = {"./aktualizr", "--gateway-http", "off", "--gateway-rvi", "on", "--gateway-socket", "on"};

  bpo::options_description description("CommandLine Options");
  description.add_options()("gateway-http", bpo::value<bool>(), "on/off the http gateway")(
      "gateway-rvi", bpo::value<bool>(), "on/off the rvi gateway")("gateway-socket", bpo::value<bool>(),
                                                                   "on/off the socket gateway");

  bpo::variables_map vm;
  bpo::store(bpo::parse_command_line(argc, argv, description), vm);
  conf.updateFromCommandLine(vm);

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
