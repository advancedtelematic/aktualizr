#include <gtest/gtest.h>

#include "aktualizr_secondary.h"
#include "primary/sotauptaneclient.h"

#include "config/config.h"
#include "httpfake.h"

std::shared_ptr<INvStorage> test_storage;
AktualizrSecondaryConfig test_config;
std::string test_sysroot;

TEST(aktualizr_secondary_uptane, getSerial) {
  test_config.pacman.sysroot = test_sysroot;
  AktualizrSecondary as(test_config, test_storage);

  EXPECT_NE(as.getSerialResp(), "");
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
  HttpFake http(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_public_key_path = "public.key";
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = kAutomatic;
  config.provision.primary_ecu_serial = "testecuserial";
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";
  config.pacman.type = kNone;

  auto storage = INvStorage::newStorage(config.storage);
  Uptane::Repository uptane(config, storage, http);
  SotaUptaneClient sota_client(config, nullptr, uptane, storage, http);
  EXPECT_TRUE(sota_client.initialize());

  std::string arch = sota_client.secondaryTreehubCredentials();
  std::string ca, cert, pkey, server_url;
  EXPECT_NO_THROW(AktualizrSecondary::extractCredentialsArchive(arch, &ca, &cert, &pkey, &server_url));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  TemporaryDirectory temp_dir;
  test_config.network.port = 0;  // random port
  test_config.storage.type = kSqlite;
  test_config.storage.sqldb_path = temp_dir.Path() / "sql.db";

  test_storage = INvStorage::newStorage(test_config.storage, temp_dir.Path());

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  test_sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
