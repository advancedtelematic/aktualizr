#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <logger.h>
#include <boost/filesystem.hpp>
#include "uptane/uptanerepository.h"
std::string credentials = "";

TEST(SotaUptaneClientTest, OneCycleUpdate) {
  system("rm -rf tests/tmp_data");
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini("tests/config_tests.toml", pt);
  pt.put("provision.provision_path", credentials);
  Config config(pt);
  Uptane::Repository repo(config);

  EXPECT_TRUE(repo.deviceRegister());
  EXPECT_TRUE(repo.ecuRegister());
  EXPECT_TRUE(repo.putManifest());

  // should not throw any exceptions
  repo.getTargets();
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerSetSeverity(LVL_trace);

  if (argc != 2) {
    exit(EXIT_FAILURE);
  }
  credentials = argv[1];
  return RUN_ALL_TESTS();
}
#endif
