#include <gtest/gtest.h>

#include "aktualizr_secondary/aktualizr_secondary.h"
#include "config.h"

boost::shared_ptr<INvStorage> storage;
AktualizrSecondaryConfig config;

TEST(aktualizr_secondary_uptane, getSerial) {
  AktualizrSecondary as(config, storage);

  EXPECT_NE(as.getSerialResp(), "");
}

TEST(aktualizr_secondary_uptane, getHwId) {
  AktualizrSecondary as(config, storage);

  EXPECT_NE(as.getHwIdResp(), "");
}

TEST(aktualizr_secondary_uptane, getPublicKey) {
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

  return RUN_ALL_TESTS();
}
#endif
