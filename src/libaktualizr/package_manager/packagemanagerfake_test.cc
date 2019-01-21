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

  PackageManagerFake fakepm(config.pacman, storage, bootloader);

  Uptane::Target target("pkg", {Uptane::Hash(Uptane::Hash::Type::kSha256, "hash")}, 1, "");
  auto result = fakepm.install(target);
  EXPECT_EQ(result.first, data::UpdateResultCode::kNeedCompletion);

  result = fakepm.finalizeInstall(target);
  EXPECT_EQ(result.first, data::UpdateResultCode::kOk);
}

#ifdef FIU_ENABLE

#include <fiu-control.h>
#include "utilities/fault_injection.h"

TEST(PackageManagerFake, FailureInjection) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PackageManager::kNone;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);

  PackageManagerFake fakepm(config.pacman, storage, nullptr);

  fiu_init(0);

  // no fault
  Uptane::Target target("pkg", {Uptane::Hash(Uptane::Hash::Type::kSha256, "hash")}, 1, "");
  auto result = fakepm.install(target);
  EXPECT_EQ(result.first, data::UpdateResultCode::kOk);

  // fault
  fiu_enable("fake_package_install", 1, nullptr, 0);
  result = fakepm.install(target);
  EXPECT_EQ(result.first, data::UpdateResultCode::kInstallFailed);
  fiu_disable("fake_package_install");

  // fault with custom data (through environment)
  fiu_enable("fake_package_install", 1, nullptr, 0);
  fault_injection_set_parameter("fault_fake_package_install_cause", "Random cause");
  result = fakepm.install(target);
  EXPECT_EQ(result.first, data::UpdateResultCode::kInstallFailed);
  EXPECT_EQ(result.second, "Random cause");
  fault_injection_set_parameter("fault_fake_package_install_cause", "");
  fiu_disable("fake_package_install");
}

#endif  // FIU_ENABLE

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
