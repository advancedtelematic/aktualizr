#include <gtest/gtest.h>

#include "aktualizr_secondary.h"
#include "primary/reportqueue.h"
#include "primary/sotauptaneclient.h"

#include "config/config.h"
#include "httpfake.h"
#include "uptane_test_common.h"

std::shared_ptr<INvStorage> test_storage;
AktualizrSecondaryConfig test_config;
std::string test_sysroot;

TEST(aktualizr_secondary_uptane, getSerial) {
  test_config.pacman.sysroot = test_sysroot;
  AktualizrSecondary as(test_config, test_storage);

  EXPECT_NE(as.getSerialResp(), Uptane::EcuSerial("hw"));
}

TEST(aktualizr_secondary_uptane, getHwId) {
  test_config.pacman.sysroot = test_sysroot;
  AktualizrSecondary as(test_config, test_storage);

  EXPECT_NE(as.getHwIdResp(), Uptane::HardwareIdentifier(""));
}

TEST(aktualizr_secondary_uptane, getPublicKey) {
  test_config.pacman.sysroot = test_sysroot;
  AktualizrSecondary as(test_config, test_storage);

  EXPECT_NO_THROW(as.getPublicKeyResp());
}

TEST(aktualizr_secondary_uptane, credentialsPassing) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kSharedCred;
  config.provision.primary_ecu_serial = "testecuserial";
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.pacman.type = PackageManager::kNone;

  auto storage = INvStorage::newStorage(config.storage);

  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  std::string arch = sota_client->secondaryTreehubCredentials();
  std::string ca, cert, pkey, server_url;
  EXPECT_NO_THROW(AktualizrSecondary::extractCredentialsArchive(arch, &ca, &cert, &pkey, &server_url));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  TemporaryDirectory temp_dir;
  test_config.network.port = 0;  // random port
  test_config.storage.path = temp_dir.Path();
  test_config.storage.type = StorageType::kSqlite;

  test_storage = INvStorage::newStorage(test_config.storage);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  test_sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
