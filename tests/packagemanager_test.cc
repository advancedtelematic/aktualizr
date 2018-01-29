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
  boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman);
  EXPECT_TRUE(pacman);
}

TEST(PackageManagerFactory, OstreeFake) {
  Config config;
  config.pacman.type = kOstreeFake;
  boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman);
  EXPECT_TRUE(pacman);
}

TEST(PackageManagerFactory, Debian) {
  Config config;
  config.pacman.type = kDebian;
  boost::shared_ptr<PackageManagerInterface> pacman = PackageManagerFactory::makePackageManager(config.pacman);
  EXPECT_FALSE(pacman);
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

  if (argc != 3) {
    std::cerr << "Error: " << argv[0]
              << " requires the paths to an OSTree sysroot generator and output as input arguments.\n";
    return EXIT_FAILURE;
  }
  boost::filesystem::path generator = argv[1];
  sysroot = argv[2];
  if (!boost::filesystem::exists(sysroot)) {
    std::string output;
    int result = Utils::shell(generator.string() + " " + sysroot.string(), &output);
    if (result != 0) {
      std::cerr << "Error: Unable to create OSTree sysroot: " << output;
      return EXIT_FAILURE;
    }
  }
  return RUN_ALL_TESTS();
}
#endif
