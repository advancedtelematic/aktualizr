#include <gtest/gtest.h>

#include <future>

#include "aktualizr_secondary/aktualizr_secondary_discovery.h"
#include "config.h"
#include "ipsecondarydiscovery.h"  // to test from the other side

#include "logging.h"

AktualizrSecondaryNetConfig config;

TEST(aktualizr_secondary_discovery, run_and_stop) {
  AktualizrSecondaryDiscovery disc{config};

  std::thread th(&AktualizrSecondaryDiscovery::run, &disc);

  disc.stop();

  th.join();
}

TEST(aktualizr_secondary_discovery, request_response) {
  AktualizrSecondaryDiscovery disc{config};

  NetworkConfig primary_netconf;
  primary_netconf.ipdiscovery_wait_seconds = 1;
  primary_netconf.ipdiscovery_host = "127.0.0.1";
  primary_netconf.ipdiscovery_port = disc.listening_port();
  IpSecondaryDiscovery primary_disc(primary_netconf);

  std::thread th(&AktualizrSecondaryDiscovery::run, &disc);

  auto secondary_configs = primary_disc.discover();

  EXPECT_EQ(secondary_configs.size(), 1);

  disc.stop();

  th.join();
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  // random port
  config.port = 0;

  return RUN_ALL_TESTS();
}
#endif
