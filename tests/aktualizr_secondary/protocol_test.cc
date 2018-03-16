#include <gtest/gtest.h>

#include <future>

#include "aktualizr_secondary/aktualizr_secondary.h"

#include "logging.h"

AktualizrSecondaryConfig conf("tests/aktualizr_secondary/config_tests.toml");

TEST(aktualizr_secondary_protocol, run_and_stop) {
  TemporaryDirectory temp_dir;
  AktualizrSecondaryConfig config = conf;
  config.network.port = 0;  // random port
  config.storage.type = kSqlite;
  config.storage.sqldb_path = temp_dir.Path() / "sql.db";

  // storage (share class with primary)
  boost::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage, temp_dir.Path());

  AktualizrSecondary as(config, storage);

  std::thread th(&AktualizrSecondary::run, &as);

  as.stop();
  th.join();
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
