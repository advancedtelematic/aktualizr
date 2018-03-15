#include <gtest/gtest.h>

#include <future>

#include "aktualizr_secondary/aktualizr_secondary.h"

#include "logging.h"

AktualizrSecondaryConfig conf("tests/aktualizr_secondary/config_tests.toml");

TEST(aktualizr_secondary_protocol, run_and_stop) {
  TemporaryFile db_file;
  AktualizrSecondaryConfig config = conf;
  config.network.port = 0;  // random port
  config.storage.type = kSqlite;
  config.storage.sqldb_path = db_file.Path();

  AktualizrSecondary as{config};

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
