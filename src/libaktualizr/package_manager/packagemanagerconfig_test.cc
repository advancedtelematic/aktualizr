#include <gtest/gtest.h>

#include "libaktualizr/config.h"
#include "logging/logging.h"

TEST(PackageManagerConfig, WriteToStream) {
  PackageConfig config;
  config.os = "amiga";
  config.fake_need_reboot = true;
  config.extra["foo"] = "bar";
  std::stringstream out;
  config.writeToStream(out);
  std::string cfg = out.str();

  ASSERT_NE(std::string::npos, cfg.find("os = \"amiga\""));
  ASSERT_NE(std::string::npos, cfg.find("fake_need_reboot = 1"));
  ASSERT_NE(std::string::npos, cfg.find("foo = \"bar\""));
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
