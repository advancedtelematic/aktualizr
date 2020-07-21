#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/polymorphic_pointer_cast.hpp>

#include "libaktualizr/packagemanagerfactory.h"
#include "libaktualizr/packagemanagerinterface.h"

#include "http/httpclient.h"
#include "logging/logging.h"
#include "package_manager/ostreemanager.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"
#include "storage/invstorage.h"
#include "uptane/uptanerepository.h"
#include "uptane_test_common.h"
#include "utilities/utils.h"

boost::filesystem::path credentials;
boost::filesystem::path sysroot;

TEST(UptaneCI, ProvisionAndPutManifest) {
  // note: see tests/shared_cred_prov_test.py which tests the same
  // functionality, with the full aktualizr binary
  TemporaryDirectory temp_dir;
  Config config("tests/config/minimal.toml");
  config.provision.provision_path = credentials;
  config.provision.mode = ProvisionMode::kSharedCredReuse;
  config.storage.path = temp_dir.Path();
  config.pacman.type = PACKAGE_MANAGER_NONE;
  config.postUpdateValues();  // re-run copy of urls

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage);
  EXPECT_NO_THROW(sota_client->initialize());
  EXPECT_TRUE(sota_client->putManifestSimple());
}

TEST(UptaneCI, CheckKeys) {
  TemporaryDirectory temp_dir;
  Config config("tests/config/minimal.toml");
  config.provision.provision_path = credentials;
  config.provision.mode = ProvisionMode::kSharedCredReuse;
  config.storage.path = temp_dir.Path();
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = sysroot;
  config.postUpdateValues();  // re-run copy of urls
  boost::filesystem::remove_all(config.storage.path);

  auto storage = INvStorage::newStorage(config.storage);

  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "", "secondary_hardware");
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage);
  EXPECT_NO_THROW(sota_client->initialize());

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

  std::map<Uptane::EcuSerial, std::shared_ptr<SecondaryInterface> >::iterator it;
  for (it = sota_client->secondaries.begin(); it != sota_client->secondaries.end(); it++) {
    std::shared_ptr<Primary::ManagedSecondary> managed_secondary =
        std::dynamic_pointer_cast<Primary::ManagedSecondary>(it->second);
    EXPECT_TRUE(managed_secondary);

    std::string public_key;
    std::string private_key;
    EXPECT_TRUE(managed_secondary->loadKeys(&public_key, &private_key));
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
