#include <gtest/gtest.h>

#include "aktualizr_info_config.h"
#include "utilities/utils.h"

TEST(aktualizr_info_config, config_initialized_values) {
  AktualizrInfoConfig conf;

  EXPECT_EQ(conf.storage.type, StorageType::kSqlite);
  EXPECT_EQ(conf.storage.path, "/var/sota");
}

TEST(aktualizr_info_config, config_toml_parsing) {
  AktualizrInfoConfig conf("config/sota-shared-cred.toml");

  EXPECT_EQ(conf.storage.type, StorageType::kSqlite);
  EXPECT_EQ(conf.storage.sqldb_path.get(conf.storage.path), "/var/sota/sql.db");
}

/* We don't normally dump the config to file, but we do write it to the log. */
TEST(aktualizr_info_config, consistent_toml_empty) {
  TemporaryDirectory temp_dir;
  AktualizrInfoConfig config1;
  std::ofstream sink1((temp_dir / "output1.toml").c_str(), std::ofstream::out);
  config1.writeToStream(sink1);

  AktualizrInfoConfig config2((temp_dir / "output1.toml").string());
  std::ofstream sink2((temp_dir / "output2.toml").c_str(), std::ofstream::out);
  config2.writeToStream(sink2);

  std::string conf_str1 = Utils::readFile((temp_dir / "output1.toml").string());
  std::string conf_str2 = Utils::readFile((temp_dir / "output2.toml").string());
  EXPECT_EQ(conf_str1, conf_str2);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
