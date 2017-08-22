#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include "fsstorage.h"

#include <boost/filesystem.hpp>
#include "httpclient.h"
#include "logger.h"
#include "ostree.h"
#include "uptane/uptanerepository.h"
std::string credentials = "";

TEST(SotaUptaneClientTest, OneCycleUpdate) {
  boost::filesystem::remove_all("tests/test_config");
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini("tests/config_tests.toml", pt);
  pt.put("provision.provision_path", credentials);
  Config config(pt);
  FSStorage storage(config);
  HttpClient http;
  Uptane::Repository repo(config, storage, http);

  EXPECT_TRUE(repo.initialize());
  Json::Value unsigned_ecu_version =
      OstreePackage::getEcu(config.uptane.primary_ecu_serial, config.ostree.sysroot).toEcuVersion(Json::nullValue);
  repo.putManifest(repo.getCurrentVersionManifests(unsigned_ecu_version));

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
