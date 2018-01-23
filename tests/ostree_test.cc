#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

#include "config.h"
#include "fsstorage.h"
#include "ostree.h"
#include "types.h"
#include "utils.h"

boost::filesystem::path sysroot;

TEST(OstreePackage, ToEcuVersion) {
  OstreePackage op("branch-name-hash", "hash", "pull_uri");
  Json::Value custom;
  custom["key"] = "value";
  Json::Value ecuver = op.toEcuVersion("ecu_serial", custom);
  EXPECT_EQ(ecuver["custom"]["key"], "value");
  EXPECT_EQ(ecuver["ecu_serial"], "ecu_serial");
  EXPECT_EQ(ecuver["installed_image"]["fileinfo"]["hashes"]["sha256"], "hash");
  EXPECT_EQ(ecuver["installed_image"]["filepath"], "branch-name-hash");
}

TEST(OstreePackage, InstallBadUri) {
  OstreePackage op("branch-name-hash", "hash", "bad_uri");
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  CryptoKey keys(boost::make_shared<FSStorage>(FSStorage(config.storage)), config);
  data::PackageManagerCredentials cred(keys);
  data::InstallOutcome result = op.install(cred, config.pacman);
  EXPECT_EQ(result.first, data::INSTALL_FAILED);
  EXPECT_EQ(result.second, "Failed to parse uri: bad_uri");
}

TEST(OstreeManager, BadSysroot) {
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = "sysroot-that-is-missing";
  EXPECT_THROW(OstreeManager ostree(config.pacman), std::runtime_error);
}

TEST(OstreeManager, ParseInstalledPackages) {
  Config config;
  config.pacman.type = kOstree;
  config.pacman.sysroot = sysroot;
  config.pacman.packages_file = "tests/test_data/package.manifest";
  OstreeManager ostree(config.pacman);
  Json::Value packages = ostree.getInstalledPackages();
  EXPECT_EQ(packages[0]["name"], "vim");
  EXPECT_EQ(packages[0]["version"], "1.0");
  EXPECT_EQ(packages[1]["name"], "emacs");
  EXPECT_EQ(packages[1]["version"], "2.0");
  EXPECT_EQ(packages[2]["name"], "bash");
  EXPECT_EQ(packages[2]["version"], "1.1");
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
