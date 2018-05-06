#include <gtest/gtest.h>

#include "aktualizr_secondary_config.h"
#include "utilities/utils.h"

TEST(aktualizr_secondary_config, config_initialized_values) {
  AktualizrSecondaryConfig conf;

  EXPECT_EQ(conf.network.port, 9030);
}

TEST(aktualizr_secondary_config, config_toml_parsing) {
  AktualizrSecondaryConfig conf("tests/config/aktualizr_secondary.toml");

  EXPECT_EQ(conf.network.port, 9031);

  EXPECT_EQ(conf.pacman.type, PackageManager::kOstree);
  EXPECT_EQ(conf.pacman.os, std::string("testos"));
  EXPECT_TRUE(conf.pacman.sysroot == boost::filesystem::path("testsysroot"));
  EXPECT_EQ(conf.pacman.ostree_server, std::string("test_server"));
  EXPECT_TRUE(conf.pacman.packages_file == boost::filesystem::path("/test_packages"));
}

TEST(aktualizr_secondary_config, consistent_toml_empty) {
  TemporaryDirectory temp_dir;
  AktualizrSecondaryConfig config1;
  config1.writeToFile((temp_dir / "output1.toml").string());
  AktualizrSecondaryConfig config2((temp_dir / "output1.toml").string());
  config2.writeToFile((temp_dir / "output2.toml").string());
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
