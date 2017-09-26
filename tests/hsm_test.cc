#include "p11.h"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include "fsstorage.h"
#include "utils.h"

TEST(HSM, ReadPublicKey) {
  Config config;

  config.p11.module = "/usr/lib/softhsm/libsofthsm2.so";
  if (!boost::filesystem::exists("/usr/lib/softhsm/")) {
    config.p11.module = "/usr/lib/x86_64-linux-gnu/softhsm/libsofthsm2.so";
  }
  FSStorage storage(config);
  std::string key_from_hsm;
  EXPECT_TRUE(storage.loadPrimaryKeys(&key_from_hsm, NULL));
  EXPECT_EQ(Utils::readFile("tests/test_data/public.key") + '\n', key_from_hsm);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif