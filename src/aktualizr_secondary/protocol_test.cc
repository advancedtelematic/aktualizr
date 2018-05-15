#include <gtest/gtest.h>

#include <future>

#include "aktualizr_secondary.h"

#include "logging/logging.h"

AktualizrSecondaryConfig conf("tests/config/aktualizr_secondary.toml");
std::string sysroot;

TEST(aktualizr_secondary_protocol, run_and_stop) {
  TemporaryDirectory temp_dir;
  AktualizrSecondaryConfig config = conf;
  config.network.port = 0;  // random port
  config.storage.type = kSqlite;
  config.storage.sqldb_path = temp_dir.Path() / "sql.db";
  config.storage.schemas_path = "config/schemas";
  config.pacman.sysroot = sysroot;
  auto storage = INvStorage::newStorage(config.storage, temp_dir.Path());

  AktualizrSecondary as(config, storage);

  std::thread th(&AktualizrSecondary::run, &as);

  as.stop();
  th.join();
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  sysroot = argv[1];
  std::cout << "SYSROOT: " << sysroot << "\n";
  return RUN_ALL_TESTS();
}
#endif
