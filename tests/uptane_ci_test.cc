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

TEST(SotaUptaneClientTest, check_keys) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini("tests/config_tests.toml", pt);
  pt.put("provision.provision_path", credentials);
  Config config(pt);
  boost::filesystem::remove_all(config.tls.certificates_directory);

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = "tests/test_uptane";
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = "/tmp/firmware.txt";
  config.uptane.secondaries.push_back(ecu_config);

  FSStorage storage(config);
  HttpClient http;
  Uptane::Repository repo(config, storage, http);
  EXPECT_TRUE(repo.initialize());

  std::string ca;
  std::string cert;
  std::string pkey;
  EXPECT_TRUE(storage.loadTlsCreds(&ca, &cert, &pkey));
  EXPECT_TRUE(ca.size() > 0);
  EXPECT_TRUE(cert.size() > 0);
  EXPECT_TRUE(pkey.size() > 0);

  std::string primary_public;
  std::string primary_private;
  EXPECT_TRUE(storage.loadPrimaryKeys(&primary_public, &primary_private));
  EXPECT_TRUE(primary_public.size() > 0);
  EXPECT_TRUE(primary_private.size() > 0);

  // There is no available public function to fetch the secondaries' public and
  // private keys, so just do it manually here.
  EXPECT_TRUE(boost::filesystem::exists(ecu_config.full_client_dir / ecu_config.ecu_public_key));
  EXPECT_TRUE(boost::filesystem::exists(ecu_config.full_client_dir / ecu_config.ecu_private_key));
  std::string sec_public = Utils::readFile((ecu_config.full_client_dir / ecu_config.ecu_public_key).string());
  std::string sec_private = Utils::readFile((ecu_config.full_client_dir / ecu_config.ecu_private_key).string());
  EXPECT_TRUE(sec_public.size() > 0);
  EXPECT_TRUE(sec_private.size() > 0);
  EXPECT_NE(sec_public, sec_private);
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
