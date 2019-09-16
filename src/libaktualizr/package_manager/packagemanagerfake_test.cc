#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <boost/filesystem.hpp>

#include "config/config.h"
#include "package_manager/packagemanagerfake.h"
#include "storage/invstorage.h"
#include "uptane/tuf.h"
#include "utilities/types.h"
#include "utilities/utils.h"

TEST(PackageManagerFake, FinalizeAfterReboot) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PackageManager::kNone;
  config.pacman.fake_need_reboot = true;
  config.bootloader.reboot_sentinel_dir = temp_dir.Path();
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::shared_ptr<Bootloader> bootloader = std::make_shared<Bootloader>(config.bootloader, *storage);

  PackageManagerFake fakepm(config.pacman, storage, bootloader, nullptr);

  Uptane::Target target("pkg", {Uptane::Hash(Uptane::Hash::Type::kSha256, "hash")}, 1, "");
  auto result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kNeedCompletion);
  storage->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);

  result = fakepm.finalizeInstall(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kOk);
}

#ifdef FIU_ENABLE

#include "utilities/fault_injection.h"

TEST(PackageManagerFake, FailureInjection) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PackageManager::kNone;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);

  PackageManagerFake fakepm(config.pacman, storage, nullptr, nullptr);

  fiu_init(0);

  // no fault
  Uptane::Target target("pkg", {Uptane::Hash(Uptane::Hash::Type::kSha256, "hash")}, 1, "");
  auto result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kOk);

  // fault
  fault_injection_enable("fake_package_install", 1, "", 0);
  result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kInstallFailed);
  fault_injection_disable("fake_package_install");

  // fault with custom data (through pid file)
  fault_injection_enable("fake_package_install", 1, "RANDOM_CAUSE", 0);
  result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode(data::ResultCode::Numeric::kInstallFailed, "RANDOM_CAUSE"));
  fault_injection_disable("fake_package_install");
}

#endif  // FIU_ENABLE

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
