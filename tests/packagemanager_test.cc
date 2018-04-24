#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <memory>

#include "config.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/fsstorage.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

boost::filesystem::path sysroot;

TEST(PackageManagerFactory, Ostree) {
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = std::make_shared<FSStorage>(config.storage);
#ifdef BUILD_OSTREE
  std::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman, storage);
  EXPECT_TRUE(pacman);
#else
  EXPECT_THROW(std::shared_ptr<PackageManagerInterface> pacman =
                   PackageManagerFactory::makePackageManager(config.pacman, storage),
               std::runtime_error);
#endif
}

TEST(PackageManagerFactory, Debian) {
  Config config;
  config.pacman.type = kDebian;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = std::make_shared<FSStorage>(config.storage);
#ifdef BUILD_DEB
  std::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman, storage);
  EXPECT_TRUE(pacman);
#else
  EXPECT_THROW(std::shared_ptr<PackageManagerInterface> pacman =
                   PackageManagerFactory::makePackageManager(config.pacman, storage),
               std::runtime_error);
#endif
}

TEST(PackageManagerFactory, None) {
  Config config;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  std::shared_ptr<INvStorage> storage = std::make_shared<FSStorage>(config.storage);
  config.pacman.type = kNone;
  std::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman, storage);
  EXPECT_TRUE(pacman);
}

TEST(PackageManagerFactory, Bad) {
  Config config;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();
  config.pacman.type = (PackageManager)-1;
  std::shared_ptr<INvStorage> storage = std::make_shared<FSStorage>(config.storage);
  std::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman, storage);
  EXPECT_FALSE(pacman);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
