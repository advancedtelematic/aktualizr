#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include <config.h>
#include <packagemanagerfactory.h>
#include <packagemanagerinterface.h>
#include <utils.h>

TEST(PackageManagerFactory, Debian_Install_Good) {
  Config config;
  config.pacman.type = kDebian;
  boost::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, boost::filesystem::path("/tmp/"));
  EXPECT_TRUE(pacman);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 0;
  Uptane::Target target("good.deb", target_json);
  Utils::writeFile(boost::filesystem::path("/tmp/targets/installed"), std::string("oldhash"), true);
  EXPECT_EQ(pacman->install(target).first, data::OK);
  EXPECT_EQ(Utils::readFile("/tmp/targets/installed"), std::string("hash"));
  EXPECT_EQ(pacman->getCurrent(), std::string("hash"));
}

TEST(PackageManagerFactory, Debian_Install_Bad) {
  Config config;
  config.pacman.type = kDebian;
  boost::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, boost::filesystem::path("/tmp/"));
  EXPECT_TRUE(pacman);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 0;
  Uptane::Target target("bad.deb", target_json);
  EXPECT_EQ(pacman->install(target).first, data::INSTALL_FAILED);
  EXPECT_EQ(pacman->install(target).second, std::string("Error installing"));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
