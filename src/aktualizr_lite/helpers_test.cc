#include <gtest/gtest.h>

#include "helpers.h"

static boost::filesystem::path test_sysroot;

TEST(version, bad_versions) {
  ASSERT_TRUE(Version("bar") < Version("foo"));
  ASSERT_TRUE(Version("1.bar") < Version("2foo"));
  ASSERT_TRUE(Version("1..0") < Version("1.1"));
  ASSERT_TRUE(Version("1.-1") < Version("1.1"));
  ASSERT_TRUE(Version("1.*bad #text") < Version("1.1"));  // ord('*') < ord('1')
}

TEST(version, good_versions) {
  ASSERT_TRUE(Version("1.0.1") < Version("1.0.1.1"));
  ASSERT_TRUE(Version("1.0.1") < Version("1.0.2"));
  ASSERT_TRUE(Version("0.9") < Version("1.0.1"));
  ASSERT_TRUE(Version("1.0.0.0") < Version("1.0.0.1"));
  ASSERT_TRUE(Version("1") < Version("1.0.0.1"));
  ASSERT_TRUE(Version("1.9.0") < Version("1.10"));
}

// Ensure we finalize an install if completed
TEST(helpers, lite_client_finalize) {
  TemporaryDirectory cfg_dir;

  Config config;
  config.storage.path = cfg_dir.Path();
  config.pacman.type = PACKAGE_MANAGER_OSTREE;
  config.pacman.sysroot = test_sysroot;
  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "deadbeef";
  target_json["custom"]["targetFormat"] = "OSTREE";
  target_json["length"] = 0;
  Uptane::Target target("test-finalize", target_json);

  setenv("OSTREE_HASH", "deadbeef", 1);
  storage->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);
  ASSERT_TRUE(target.MatchHash(LiteClient(config).primary->getCurrent().hashes()[0]));

  config = Config();  // Create a new config since LiteClient std::move's it
  config.storage.path = cfg_dir.Path();
  config.pacman.type = "ostree";
  config.pacman.sysroot = test_sysroot;

  setenv("OSTREE_HASH", "abcd", 1);
  storage->savePrimaryInstalledVersion(target, InstalledVersionUpdateMode::kPending);
  ASSERT_FALSE(target.MatchHash(LiteClient(config).primary->getCurrent().hashes()[0]));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot.\n";
    return EXIT_FAILURE;
  }

  TemporaryDirectory temp_dir;
  // Utils::copyDir doesn't work here. Complaints about non existent symlink path
  int r = system((std::string("cp -r ") + argv[1] + std::string(" ") + temp_dir.PathString()).c_str());
  if (r != 0) {
    return -1;
  }
  test_sysroot = (temp_dir.Path() / "ostree_repo").string();

  return RUN_ALL_TESTS();
}
#endif
