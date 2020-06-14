#include <gtest/gtest.h>
#include <libaktualizr/utils.h>

#include "aktualizr_secondary_config.h"

TEST(aktualizr_secondary_config, config_initialized_values) {
  AktualizrSecondaryConfig conf;

  EXPECT_EQ(conf.network.port, 9030);
}

TEST(aktualizr_secondary_config, config_toml_parsing) {
  AktualizrSecondaryConfig conf("tests/config/aktualizr_secondary.toml");

  EXPECT_EQ(conf.network.port, 9031);
#ifdef BUILD_OSTREE
  EXPECT_EQ(conf.pacman.type, PACKAGE_MANAGER_OSTREE);
#else
  EXPECT_EQ(conf.pacman.type, PACKAGE_MANAGER_NONE);
#endif
  EXPECT_EQ(conf.pacman.os, std::string("testos"));
  EXPECT_EQ(conf.pacman.sysroot, boost::filesystem::path("testsysroot"));
  EXPECT_EQ(conf.pacman.ostree_server, std::string("test_server"));
  EXPECT_EQ(conf.pacman.packages_file, boost::filesystem::path("/test_packages"));
}

/* We don't normally dump the config to file, but we do write it to the log. */
TEST(aktualizr_secondary_config, consistent_toml_empty) {
  TemporaryDirectory temp_dir;
  AktualizrSecondaryConfig config1;
  std::ofstream sink1((temp_dir / "output1.toml").c_str(), std::ofstream::out);
  config1.writeToStream(sink1);

  AktualizrSecondaryConfig config2((temp_dir / "output1.toml").string());
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
