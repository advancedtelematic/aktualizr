#include <gtest/gtest.h>

#include "bootloader.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

/* Check that the reboot detection feature works */
TEST(bootloader, detectReboot) {
  TemporaryDirectory temp_dir;
  StorageConfig storage_config;
  storage_config.path = temp_dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(storage_config);

  BootloaderConfig boot_config;
  boot_config.reboot_sentinel_dir = temp_dir.Path();
  Bootloader bootloader(boot_config, storage);

  ASSERT_TRUE(bootloader.supportRebootDetection());

  // nothing happened should not flag a reboot
  ASSERT_FALSE(bootloader.rebootDetected());

  // we flagged an expected reboot but did not reboot yet
  bootloader.rebootFlagSet();
  ASSERT_FALSE(bootloader.rebootDetected());

  // simulate the reboot: loss of a temporary file
  boost::filesystem::remove(boot_config.reboot_sentinel_dir / boot_config.reboot_sentinel_name);
  ASSERT_TRUE(bootloader.rebootDetected());

  // should still flag if not cleared
  ASSERT_TRUE(bootloader.rebootDetected());

  // stop flagging when cleared
  bootloader.rebootFlagClear();
  ASSERT_FALSE(bootloader.rebootDetected());
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
