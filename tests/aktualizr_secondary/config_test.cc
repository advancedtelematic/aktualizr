#include <gtest/gtest.h>

#include "aktualizr_secondary/aktualizr_secondary_config.h"
#include "utils.h"

TEST(aktualizr_secondary_config, config_initialized_values) {
  AktualizrSecondaryConfig conf;

  EXPECT_EQ(conf.network.port, 9030);
}

TEST(aktualizr_secondary_config, config_toml_parsing) {
  AktualizrSecondaryConfig conf("tests/aktualizr_secondary/config_tests.toml");

  EXPECT_EQ(conf.network.port, 9031);
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
