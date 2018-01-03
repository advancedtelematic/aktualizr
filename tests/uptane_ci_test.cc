#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/polymorphic_pointer_cast.hpp>
#include <boost/shared_ptr.hpp>

#include "fsstorage.h"
#include "httpclient.h"
#include "logging.h"
#include "ostree.h"
#include "sotauptaneclient.h"
#include "uptane/managedsecondary.h"
#include "uptane/uptanerepository.h"
#include "utils.h"

boost::filesystem::path credentials = "";

TEST(UptaneCI, OneCycleUpdate) {
  TemporaryDirectory temp_dir;
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini("tests/config_tests.toml", pt);
  pt.put("provision.provision_path", credentials);
  pt.put("storage.path", temp_dir.Path());
  pt.put("uptane.metadata_path", temp_dir.Path());
  Config config(pt);
  boost::shared_ptr<INvStorage> storage(new FSStorage(config.storage));
  HttpClient http;
  Uptane::Repository repo(config, storage, http);

  EXPECT_TRUE(repo.initialize());

  Json::Value result = Json::arrayValue;
  std::string hash = OstreePackage::getCurrent(config.ostree.sysroot);
  std::string refname(std::string("unknown-") + hash);
  Json::Value unsigned_ecu_version =
      OstreePackage(refname, hash, "").toEcuVersion(repo.getPrimaryEcuSerial(), Json::nullValue);

  result.append(repo.signVersionManifest(unsigned_ecu_version));
  EXPECT_TRUE(repo.putManifest(result));

  // should not throw any exceptions
  repo.getTargets();
}

TEST(UptaneCI, CheckKeys) {
  TemporaryDirectory temp_dir;
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini("tests/config_tests.toml", pt);
  pt.put("provision.provision_path", credentials);
  pt.put("storage.path", temp_dir.Path());
  Config config(pt);
  boost::filesystem::remove_all(config.storage.path);

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path();
  ecu_config.ecu_serial = "";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = (temp_dir / "firmware.txt").string();
  ecu_config.target_name_path = (temp_dir / "firmware_name.txt").string();
  ecu_config.metadata_path = (temp_dir / "secondary_metadata").string();
  config.uptane.secondary_configs.push_back(ecu_config);

  boost::shared_ptr<INvStorage> storage(new FSStorage(config.storage));
  HttpClient http;
  Uptane::Repository repo(config, storage, http);
  SotaUptaneClient sota_client(config, NULL, repo, storage, http);
  EXPECT_TRUE(repo.initialize());

  std::string ca;
  std::string cert;
  std::string pkey;
  EXPECT_TRUE(storage->loadTlsCreds(&ca, &cert, &pkey));
  EXPECT_TRUE(ca.size() > 0);
  EXPECT_TRUE(cert.size() > 0);
  EXPECT_TRUE(pkey.size() > 0);

  std::string primary_public;
  std::string primary_private;
  EXPECT_TRUE(storage->loadPrimaryKeys(&primary_public, &primary_private));
  EXPECT_TRUE(primary_public.size() > 0);
  EXPECT_TRUE(primary_private.size() > 0);

  std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::iterator it;
  for (it = sota_client.secondaries.begin(); it != sota_client.secondaries.end(); it++) {
    if (it->second->sconfig.secondary_type != Uptane::kVirtual &&
        it->second->sconfig.secondary_type != Uptane::kLegacy) {
      continue;
    }
    boost::shared_ptr<Uptane::ManagedSecondary> managed =
        boost::polymorphic_pointer_downcast<Uptane::ManagedSecondary>(it->second);
    std::string public_key;
    std::string private_key;
    EXPECT_TRUE(managed->loadKeys(&public_key, &private_key));
    EXPECT_TRUE(public_key.size() > 0);
    EXPECT_TRUE(private_key.size() > 0);
    EXPECT_NE(public_key, private_key);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires a path to a credentials archive as an input argument.\n";
    exit(EXIT_FAILURE);
  }
  credentials = argv[1];
  return RUN_ALL_TESTS();
}
#endif
