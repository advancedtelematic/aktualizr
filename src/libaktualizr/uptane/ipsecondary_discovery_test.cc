/**
 * \file
 */

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "config/config.h"
#include "logging/logging.h"
#include "test_utils.h"
#include "uptane/ipsecondarydiscovery.h"

std::string port;

TEST(ipsecondary_discovery, test_discovery) {
  NetworkConfig conf;
  conf.ipdiscovery_host = "::ffff:127.0.0.1";
  conf.ipdiscovery_port = std::stoi(port);
  conf.ipdiscovery_wait_seconds = 12;
  IpSecondaryDiscovery discoverer(conf);
  std::vector<Uptane::SecondaryConfig> secondaries = discoverer.discover();
  EXPECT_EQ(secondaries.size(), 2);
  EXPECT_EQ(secondaries[0].ecu_serial, "test_ecu");
  EXPECT_EQ(secondaries[0].ecu_hardware_id, "test_hardware_id");
  EXPECT_EQ(secondaries[1].ecu_serial, "test_ecu2");
  EXPECT_EQ(secondaries[1].ecu_hardware_id, "test_hardware_id2");
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  port = TestUtils::getFreePort();
  TestHelperProcess server_process("tests/fake_discovery/discovery_secondary.py", port);

  sleep(3);

  return RUN_ALL_TESTS();
}
#endif
