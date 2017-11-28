#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>

#include "fsstorage.h"
#include "httpclient.h"
#include "logger.h"
#include "ostree.h"
#include "uptane/uptanerepository.h"

const std::string test_dir = "tests/test_ci_uptane";
std::string credentials = "";

TEST(SotaUptaneClientTest, OneCycleUpdate) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini("tests/config_tests.toml", pt);
  pt.put("provision.provision_path", credentials);
  pt.put("tls.certificates_directory", test_dir);
  pt.put("uptane.metadata_path", test_dir);
  Config config(pt);
  FSStorage storage(config);
  HttpClient http;
  Uptane::Repository repo(config, storage, http);

  EXPECT_TRUE(repo.initialize());

  std::string hash = OstreePackage::getCurrent(config.ostree.sysroot);
  std::string refname(std::string("unknown-") + hash);
  Json::Value unsigned_ecu_version =
      OstreePackage(refname, hash, "").toEcuVersion(repo.getPrimaryEcuSerial(), Json::nullValue);

  EXPECT_TRUE(repo.putManifest(repo.signVersionManifest(unsigned_ecu_version)));

  // should not throw any exceptions
  repo.getTargets();
  boost::filesystem::remove_all(test_dir);
}

TEST(SotaUptaneClientTest, check_keys) {
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini("tests/config_tests.toml", pt);
  pt.put("provision.provision_path", credentials);
  pt.put("tls.certificates_directory", test_dir);
  Config config(pt);
  boost::filesystem::remove_all(config.tls.certificates_directory);

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = test_dir;
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key_ = "sec.priv";
  ecu_config.ecu_public_key_ = "sec.pub";
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
  EXPECT_TRUE(boost::filesystem::exists(ecu_config.ecu_public_key()));
  EXPECT_TRUE(boost::filesystem::exists(ecu_config.ecu_private_key()));
  std::string sec_public = Utils::readFile(ecu_config.ecu_public_key());
  std::string sec_private = Utils::readFile(ecu_config.ecu_private_key());
  EXPECT_TRUE(sec_public.size() > 0);
  EXPECT_TRUE(sec_private.size() > 0);
  EXPECT_NE(sec_public, sec_private);
  boost::filesystem::remove_all(test_dir);
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
