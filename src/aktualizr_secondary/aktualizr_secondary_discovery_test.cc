#include <gtest/gtest.h>

#include <future>

#include "aktualizr_secondary_discovery.h"
#include "config/config.h"
#include "ipsecondarydiscovery.h"  // to test from the other side

#include "logging/logging.h"

std::shared_ptr<INvStorage> storage;
AktualizrSecondaryConfig config;
std::string sysroot;

TEST(aktualizr_secondary_discovery, run_and_stop) {
  config.pacman.sysroot = sysroot;
  AktualizrSecondary as(config, storage);
  AktualizrSecondaryDiscovery disc{config.network, as};

  std::thread th(&AktualizrSecondaryDiscovery::run, &disc);

  disc.stop();

  th.join();
}

TEST(aktualizr_secondary_discovery, request_response) {
  config.pacman.sysroot = sysroot;
  AktualizrSecondary as(config, storage);
  AktualizrSecondaryDiscovery disc{config.network, as};

  NetworkConfig primary_netconf;
  primary_netconf.ipdiscovery_wait_seconds = 1;
  primary_netconf.ipdiscovery_host = "127.0.0.1";
  primary_netconf.ipdiscovery_port = static_cast<in_port_t>(disc.listening_port());
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

  TemporaryDirectory temp_dir;
  config.network.port = 0;  // random port
  config.storage.type = StorageType::kSqlite;
  config.storage.path = temp_dir.Path();
  config.storage.sqldb_path = temp_dir.Path() / "sql.db";

  storage = INvStorage::newStorage(config.storage);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  sysroot = argv[1];
  std::cout << "SYSROOT: " << sysroot << "\n";
  return RUN_ALL_TESTS();
}
#endif
