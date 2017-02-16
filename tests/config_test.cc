#define BOOST_TEST_MODULE test_config

#include <boost/program_options.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>

#include <string>

#include "config.h"

namespace bpo = boost::program_options;
namespace utf = boost::unit_test;
extern bpo::variables_map parse_options(int argc, char *argv[]);

BOOST_AUTO_TEST_CASE(config_initialized_values) {
  Config conf;

  BOOST_CHECK_EQUAL(conf.core.server, "http://127.0.0.1:8080");
  BOOST_CHECK_EQUAL(conf.core.polling, true);
  BOOST_CHECK_EQUAL(conf.core.polling_sec, 10);

  BOOST_CHECK_EQUAL(conf.auth.server, "http://127.0.0.1:9001");
  BOOST_CHECK_EQUAL(conf.auth.client_id, "client-id");
  BOOST_CHECK_EQUAL(conf.auth.client_secret, "client-secret");

  BOOST_CHECK_EQUAL(conf.device.uuid, "123e4567-e89b-12d3-a456-426655440000");
  BOOST_CHECK_EQUAL(conf.device.packages_dir, "/tmp/");

  BOOST_CHECK_EQUAL(conf.gateway.http, true);
  BOOST_CHECK_EQUAL(conf.gateway.rvi, false);
}

BOOST_AUTO_TEST_CASE(config_toml_parsing) {
  Config conf;
  conf.updateFromToml("config/config.toml.example");

  BOOST_CHECK_EQUAL(conf.core.server, "https://url.com");
  BOOST_CHECK_EQUAL(conf.core.polling, true);
  BOOST_CHECK_EQUAL(conf.core.polling_sec, 10);

  BOOST_CHECK_EQUAL(conf.auth.server, "https://url.com");
  BOOST_CHECK_EQUAL(conf.auth.client_id, "thisisaclientid");
  BOOST_CHECK_EQUAL(conf.auth.client_secret, "thisisaclientsecret");

  BOOST_CHECK_EQUAL(conf.device.uuid, "bc50fa11-eb93-41c0-b0fa-5ce56affa63e");
  BOOST_CHECK_EQUAL(conf.device.packages_dir, "/tmp/");
}

BOOST_AUTO_TEST_CASE(config_toml_parsing_empty_file) {
  Config conf;
  conf.updateFromToml("Testing/config.toml");

  BOOST_CHECK_EQUAL(conf.core.server, "http://127.0.0.1:8080");
  BOOST_CHECK_EQUAL(conf.core.polling, true);
  BOOST_CHECK_EQUAL(conf.core.polling_sec, 10);

  BOOST_CHECK_EQUAL(conf.auth.server, "http://127.0.0.1:9001");
  BOOST_CHECK_EQUAL(conf.auth.client_id, "client-id");
  BOOST_CHECK_EQUAL(conf.auth.client_secret, "client-secret");

  BOOST_CHECK_EQUAL(conf.device.uuid, "123e4567-e89b-12d3-a456-426655440000");
  BOOST_CHECK_EQUAL(conf.device.packages_dir, "/tmp/");

  BOOST_CHECK_EQUAL(conf.gateway.http, true);
  BOOST_CHECK_EQUAL(conf.gateway.rvi, false);
}

BOOST_AUTO_TEST_CASE(config_cmdl_parsing) {
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

  BOOST_CHECK_EQUAL(conf.gateway.http, false);
  BOOST_CHECK_EQUAL(conf.gateway.rvi, true);
  BOOST_CHECK_EQUAL(conf.gateway.socket, true);
}
