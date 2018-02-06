#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

#include <config.h>
#include <packagemanagerfactory.h>
#include <packagemanagerinterface.h>
#include <utils.h>

boost::filesystem::path sysroot;

TEST(PackageManagerFactory, Ostree) {
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
#ifdef BUILD_OSTREE
  boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman);
  EXPECT_TRUE(pacman);
#else
  EXPECT_THROW(
      boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman),
      std::runtime_error);
#endif
}

TEST(PackageManagerFactory, Debian) {
  Config config;
  config.pacman.type = kDebian;
#ifdef BUILD_DEB
  boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman);
  EXPECT_TRUE(pacman);
#else
  EXPECT_THROW(
      boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman),
      std::runtime_error);
#endif
}

TEST(PackageManagerFactory, None) {
  Config config;
  config.pacman.type = kNone;
  boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman);
  EXPECT_TRUE(pacman);
}

TEST(PackageManagerFactory, Bad) {
  Config config;
  config.pacman.type = (PackageManager)-1;
  boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman);
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
