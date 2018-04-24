#include <gtest/gtest.h>

#include "aktualizr_secondary.h"
#include "sotauptaneclient.h"

#include "config.h"
#include "httpfake.h"

std::shared_ptr<INvStorage> storage;
AktualizrSecondaryConfig config;
std::string sysroot;

TEST(aktualizr_secondary_uptane, getSerial) {
  config.pacman.sysroot = sysroot;
  AktualizrSecondary as(config, storage);

  EXPECT_NE(as.getSerialResp(), "");
}

TEST(aktualizr_secondary_uptane, getHwId) {
  config.pacman.sysroot = sysroot;
  AktualizrSecondary as(config, storage);

  EXPECT_NE(as.getHwIdResp(), "");
}

TEST(aktualizr_secondary_uptane, getPublicKey) {
  config.pacman.sysroot = sysroot;
  AktualizrSecondary as(config, storage);

  KeyType type;
  std::string key;
  std::tie(type, key) = as.getPublicKeyResp();

  EXPECT_NE(type, kUnknownKey);
  EXPECT_NE(key, "");
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
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";
  config.uptane.primary_ecu_serial = "testecuserial";
  config.pacman.type = kNone;

  auto storage = INvStorage::newStorage(config.storage);
  Uptane::Repository uptane(config, storage, http);
  SotaUptaneClient sota_client(config, NULL, uptane, storage, http);
  EXPECT_TRUE(uptane.initialize());

  std::string arch = sota_client.secondaryTreehubCredentials();
  std::string ca, cert, pkey, server_url;
  EXPECT_NO_THROW(AktualizrSecondary::extractCredentialsArchive(arch, &ca, &cert, &pkey, &server_url));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  TemporaryDirectory temp_dir;
  config.network.port = 0;  // random port
  config.storage.type = kSqlite;
  config.storage.sqldb_path = temp_dir.Path() / "sql.db";
  config.storage.schemas_path = "config/schemas";
  storage = INvStorage::newStorage(config.storage, temp_dir.Path());

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
