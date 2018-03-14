#include <gtest/gtest.h>

#include <future>

#include "aktualizr_secondary/aktualizr_secondary.h"

#include "logging.h"

TEST(aktualizr_secondary_protocol, run_and_stop) {
  AktualizrSecondaryConfig config;
  config.network.port = 0;  // random port

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
