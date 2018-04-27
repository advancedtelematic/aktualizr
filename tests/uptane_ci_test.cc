#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/polymorphic_pointer_cast.hpp>

#include "storage/fsstorage.h"
#include "logging/logging.h"
#include "package_manager/ostreemanager.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "primary/sotauptaneclient.h"
#include "uptane/managedsecondary.h"
#include "uptane/uptanerepository.h"
#include "http/httpclient.h"
#include "utilities/utils.h"

boost::filesystem::path credentials;
boost::filesystem::path sysroot;

TEST(UptaneCI, OneCycleUpdate) {
  TemporaryDirectory temp_dir;
  Config config("tests/config/minimal.toml");
  config.provision.provision_path = credentials;
  config.provision.mode = kAutomatic;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = temp_dir.Path();
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.postUpdateValues();  // re-run copy of urls

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  HttpClient http;
  Uptane::Repository repo(config, storage, http);
  SotaUptaneClient sota_client(config, NULL, repo, storage, http);
  EXPECT_TRUE(repo.initialize());
  EXPECT_TRUE(repo.putManifest(sota_client.AssembleManifest()));
  // should not throw any exceptions
  repo.getTargets();
}

TEST(UptaneCI, CheckKeys) {
  TemporaryDirectory temp_dir;
  Config config("tests/config/minimal.toml");
  config.provision.provision_path = credentials;
  config.provision.mode = kAutomatic;
  config.storage.path = temp_dir.Path();
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.postUpdateValues();  // re-run copy of urls
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

  auto storage = INvStorage::newStorage(config.storage);
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

  std::map<std::string, std::shared_ptr<Uptane::SecondaryInterface> >::iterator it;
  for (it = sota_client.secondaries.begin(); it != sota_client.secondaries.end(); it++) {
    if (it->second->sconfig.secondary_type != Uptane::kVirtual &&
        it->second->sconfig.secondary_type != Uptane::kLegacy) {
      continue;
    }
    std::shared_ptr<Uptane::ManagedSecondary> managed =
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

  if (argc != 3) {
    std::cerr << "Error: " << argv[0]
              << " requires a path to a credentials archive and an OSTree sysroot as input arguments.\n";
    exit(EXIT_FAILURE);
  }
  credentials = argv[1];
  sysroot = argv[2];
  return RUN_ALL_TESTS();
}
#endif
