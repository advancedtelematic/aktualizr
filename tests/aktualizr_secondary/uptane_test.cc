#include <gtest/gtest.h>

#include "aktualizr_secondary/aktualizr_secondary.h"
#include "config.h"

boost::shared_ptr<INvStorage> storage;
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
