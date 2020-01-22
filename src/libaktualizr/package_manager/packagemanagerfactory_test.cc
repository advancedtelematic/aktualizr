#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <memory>

#include "config/config.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

boost::filesystem::path sysroot;

/* Support OSTree as a package manager. */
TEST(PackageManagerFactory, Ostree) {
  Config config;
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = sysroot;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
#ifdef BUILD_OSTREE
  std::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  EXPECT_TRUE(pacman);
#else
  EXPECT_THROW(std::shared_ptr<PackageManagerInterface> pacman =
                   PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr),
               std::runtime_error);
#endif
}

TEST(PackageManagerFactory, Debian) {
  Config config;
  config.pacman.type = PACKAGE_MANAGER_DEBIAN;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
#ifdef BUILD_DEB
  std::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  EXPECT_TRUE(pacman);
#else
  EXPECT_THROW(std::shared_ptr<PackageManagerInterface> pacman =
                   PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr),
               std::runtime_error);
#endif
}

TEST(PackageManagerFactory, None) {
  Config config;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  config.pacman.type = PACKAGE_MANAGER_NONE;
  std::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  EXPECT_TRUE(pacman);
}

TEST(PackageManagerFactory, Bad) {
  Config config;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  config.pacman.type = "bad";
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  std::shared_ptr<PackageManagerInterface> pacman =
      PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr);
  EXPECT_FALSE(pacman);
}

#include "package_manager/packagemanagerfake.h"

TEST(PackageManagerFactory, Register) {
  // a package manager cannot be registered twice
  EXPECT_THROW(PackageManagerFactory::registerPackageManager(
                   "none",
                   [](const PackageConfig&, const BootloaderConfig&, const std::shared_ptr<INvStorage>&,
                      const std::shared_ptr<HttpInterface>&) -> PackageManagerInterface* {
                     throw std::runtime_error("unimplemented");
                   }),
               std::runtime_error);

  PackageManagerFactory::registerPackageManager(
      "new",
      [](const PackageConfig& pconfig, const BootloaderConfig& bconfig, const std::shared_ptr<INvStorage>& storage,
         const std::shared_ptr<HttpInterface>& http) -> PackageManagerInterface* {
        return new PackageManagerFake(pconfig, bconfig, storage, http);
      });

  Config config;
  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  config.pacman.type = "new";
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);

  EXPECT_NE(PackageManagerFactory::makePackageManager(config.pacman, config.bootloader, storage, nullptr), nullptr);
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
