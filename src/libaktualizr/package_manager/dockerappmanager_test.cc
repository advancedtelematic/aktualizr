#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include "config/config.h"
#include "package_manager/packagemanagerfactory.h"
#include "package_manager/packagemanagerinterface.h"
#include "storage/invstorage.h"

boost::filesystem::path test_sysroot;

TEST(DockerAppManager, PackageManager_Factory_Good) {
  Config config;
  config.pacman.type = PackageManager::kOstreeDockerApp;
  config.pacman.sysroot = test_sysroot;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  auto pacman = PackageManagerFactory::makePackageManager(config.pacman, storage, nullptr, nullptr);
  EXPECT_TRUE(pacman);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  test_sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif
