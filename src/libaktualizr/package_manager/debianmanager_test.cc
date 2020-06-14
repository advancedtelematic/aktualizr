#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <memory>

#include <libaktualizr/config.h>
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"
#include <libaktualizr/utils.h>

TEST(PackageManagerFactory, Debian_Install_Good) {
  Config config;
  config.pacman.type = PACKAGE_MANAGER_DEBIAN;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  EXPECT_TRUE(pacman);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 2;
  target_json["custom"]["ecuIdentifiers"]["primary_serial"]["hardwareId"] = "primary_hwid";
  Uptane::Target target("good.deb", target_json);

  storage->storeEcuSerials({{Uptane::EcuSerial("primary_serial"), Uptane::HardwareIdentifier("primary_hwid")}});

  Json::Value target_json_test;
  target_json_test["hashes"]["sha256"] = "hash_old";
  target_json_test["length"] = 2;
  Uptane::Target target_test("test.deb", target_json_test);
  storage->savePrimaryInstalledVersion(target_test, InstalledVersionUpdateMode::kCurrent);
  auto fhandle = pacman->createTargetFile(target);
  fhandle << "ab";
  fhandle.close();

  EXPECT_EQ(pacman->install(target).result_code.num_code, data::ResultCode::Numeric::kOk);
  EXPECT_TRUE(pacman->getCurrent().MatchTarget(target));
}

TEST(PackageManagerFactory, Debian_Install_Bad) {
  Config config;
  config.pacman.type = PACKAGE_MANAGER_DEBIAN;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  EXPECT_TRUE(pacman);
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "hash";
  target_json["length"] = 2;
  target_json["custom"]["ecuIdentifiers"]["primary_serial"]["hardwareId"] = "primary_hwid";
  Uptane::Target target("bad.deb", target_json);

  auto fhandle = pacman->createTargetFile(target);
  fhandle << "ab";
  fhandle.close();

  auto result = pacman->install(target);
  EXPECT_EQ(result.result_code.num_code, data::ResultCode::Numeric::kInstallFailed);
  EXPECT_EQ(result.description, "Error installing");
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
