#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <boost/filesystem.hpp>

#include "config/config.h"
#include "httpfake.h"
#include "package_manager/packagemanagerfake.h"
#include "storage/invstorage.h"
#include "uptane/tuf.h"
#include "utilities/types.h"
#include "utilities/utils.h"

/*
 * Verify a stored target.
 * Verify that a target is unavailable.
 * Reject a target whose hash does not match the metadata.
 * Reject an oversized target.
 * Reject an incomplete target.
 */
TEST(PackageManagerFake, Verify) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PackageManager::kNone;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);

  Uptane::EcuMap primary_ecu{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")}};
  const int length = 4;
  uint8_t content[length];
  memcpy(content, "good", length);
  MultiPartSHA256Hasher hasher;
  hasher.update(content, length);
  const std::string hash = hasher.getHexDigest();
  Uptane::Target target("some-pkg", primary_ecu, {Uptane::Hash(Uptane::Hash::Type::kSha256, hash)}, length, "");

  PackageManagerFake fakepm(config.pacman, config.bootloader, storage, nullptr);
  // Target is not yet available.
  EXPECT_EQ(fakepm.verifyTarget(target), TargetStatus::kNotFound);

  // Target has a bad hash.
  auto whandle = storage->allocateTargetFile(false, target);
  uint8_t content_bad[length + 1];
  memset(content_bad, 0, length + 1);
  EXPECT_EQ(whandle->wfeed(content_bad, length), length);
  whandle->wcommit();
  EXPECT_EQ(fakepm.verifyTarget(target), TargetStatus::kHashMismatch);

  // Target is oversized.
  whandle = storage->allocateTargetFile(false, target);
  EXPECT_EQ(whandle->wfeed(content_bad, length + 1), length + 1);
  whandle->wcommit();
  EXPECT_EQ(fakepm.verifyTarget(target), TargetStatus::kOversized);

  // Target is incomplete.
  whandle = storage->allocateTargetFile(false, target);
  EXPECT_EQ(whandle->wfeed(content, length - 1), length - 1);
  whandle->wcommit();
  EXPECT_EQ(fakepm.verifyTarget(target), TargetStatus::kIncomplete);

  // Target is good.
  whandle = storage->allocateTargetFile(false, target);
  EXPECT_EQ(whandle->wfeed(content, length), length);
  whandle->wcommit();
  EXPECT_EQ(fakepm.verifyTarget(target), TargetStatus::kGood);
}

TEST(PackageManagerFake, FinalizeAfterReboot) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PackageManager::kNone;
  config.pacman.fake_need_reboot = true;
  config.bootloader.reboot_sentinel_dir = temp_dir.Path();
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::shared_ptr<Bootloader> bootloader = std::make_shared<Bootloader>(config.bootloader, *storage);

  PackageManagerFake fakepm(config.pacman, config.bootloader, storage, nullptr);

  Uptane::EcuMap primary_ecu;
  Uptane::Target target("pkg", primary_ecu, {Uptane::Hash(Uptane::Hash::Type::kSha256, "hash")}, 1, "");
  auto result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kNeedCompletion);
  storage->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);

  fakepm.completeInstall();

  result = fakepm.finalizeInstall(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kOk);
}

#ifdef FIU_ENABLE

#include "utilities/fault_injection.h"

TEST(PackageManagerFake, DownloadFailureInjection) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PackageManager::kNone;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Uptane::Fetcher uptane_fetcher(config, http);
  KeyManager keys(storage, config.keymanagerConfig());

  PackageManagerFake fakepm(config.pacman, config.bootloader, storage, http);

  fault_injection_init();

  // no fault
  Uptane::EcuMap primary_ecu{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")}};
  Uptane::Target target("pkg", primary_ecu, {Uptane::Hash(Uptane::Hash::Type::kSha256, "hash")}, 0, "");
  EXPECT_TRUE(fakepm.fetchTarget(target, uptane_fetcher, keys, nullptr, nullptr));

  // fault
  fault_injection_enable("fake_package_download", 1, "", 0);
  EXPECT_FALSE(fakepm.fetchTarget(target, uptane_fetcher, keys, nullptr, nullptr));
  fault_injection_disable("fake_package_download");

  // fault with custom data (through pid file). Unfortunately no easy way to
  // test the custom emssage.
  fault_injection_enable("fake_package_download", 1, "RANDOM_DOWNLOAD_CAUSE", 0);
  EXPECT_FALSE(fakepm.fetchTarget(target, uptane_fetcher, keys, nullptr, nullptr));
  fault_injection_disable("fake_package_download");
}

TEST(PackageManagerFake, InstallFailureInjection) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.type = PackageManager::kNone;
  config.storage.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);

  PackageManagerFake fakepm(config.pacman, config.bootloader, storage, nullptr);

  fault_injection_init();

  // no fault
  Uptane::EcuMap primary_ecu{{Uptane::EcuSerial("primary"), Uptane::HardwareIdentifier("primary_hw")}};
  Uptane::Target target("pkg", primary_ecu, {Uptane::Hash(Uptane::Hash::Type::kSha256, "hash")}, 1, "");
  auto result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kOk);

  // fault
  fault_injection_enable("fake_package_install", 1, "", 0);
  result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode::Numeric::kInstallFailed);
  fault_injection_disable("fake_package_install");

  // fault with custom data (through pid file)
  fault_injection_enable("fake_package_install", 1, "RANDOM_INSTALL_CAUSE", 0);
  result = fakepm.install(target);
  EXPECT_EQ(result.result_code, data::ResultCode(data::ResultCode::Numeric::kInstallFailed, "RANDOM_INSTALL_CAUSE"));
  fault_injection_disable("fake_package_install");
}

#endif  // FIU_ENABLE

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
#endif
