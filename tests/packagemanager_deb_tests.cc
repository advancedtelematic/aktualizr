#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <memory>

#include "config/config.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/fsstorage.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

TEST(PackageManagerFactory, Debian_Install_Good) {
  Config config;
  config.pacman.type = kDebian;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  std::shared_ptr<INvStorage> storage = std::make_shared<FSStorage>(config.storage);
  std::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman, storage);
  EXPECT_TRUE(pacman);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 0;
  Uptane::Target target("good.deb", target_json);

  Json::Value target_json_test;
  target_json_test["hashes"]["sha256"] = "hash_old";
  target_json_test["length"] = 0;
  Uptane::Target target_test("test.deb", target_json_test);
  storage->saveInstalledVersion(target_test);

  std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, "good.deb", 2);
  std::stringstream("ab") >> *fhandle;

  EXPECT_EQ(pacman->install(target).first, data::OK);
  std::vector<Uptane::Target> versions_loaded;
  EXPECT_EQ(pacman->getCurrent(), target);
}

TEST(PackageManagerFactory, Debian_Install_Bad) {
  Config config;
  config.pacman.type = kDebian;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = std::make_shared<FSStorage>(config.storage);
  std::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman, storage);
  EXPECT_TRUE(pacman);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 0;
  Uptane::Target target("bad.deb", target_json);

  std::unique_ptr<StorageTargetWHandle> fhandle = storage->allocateTargetFile(false, "bad.deb", 2);
  std::stringstream("ab") >> *fhandle;

  EXPECT_EQ(pacman->install(target).first, data::INSTALL_FAILED);
  EXPECT_EQ(pacman->install(target).second, std::string("Error installing"));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
