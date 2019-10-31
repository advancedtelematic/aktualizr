#include <gtest/gtest.h>

#include <string>

#include <boost/filesystem.hpp>
#include <boost/process.hpp>

#include "config/config.h"
#include "package_manager/ostreemanager.h"
#include "storage/invstorage.h"
#include "utilities/utils.h"

#include "test_utils.h"

boost::filesystem::path test_sysroot;

static std::string treehub_server = "http://127.0.0.1:";
static std::string new_rev;

/* Reject bad OSTree server URIs. */
TEST(OstreeManager, CorruptRepo) {
  TemporaryDirectory temp_dir;
  Config config;
  config.pacman.ostree_server = "bad-url";
  config.pacman.type = PackageManager::kOstree;
  config.pacman.sysroot = test_sysroot;
  config.storage.path = temp_dir.Path();

  std::shared_ptr<INvStorage> storage = INvStorage::newStorage(config.storage);
  KeyManager keys(storage, config.keymanagerConfig());
  keys.loadKeys();
  Json::Value target_json_test;
  target_json_test["hashes"]["sha256"] = new_rev;
  target_json_test["length"] = 0;
  Uptane::Target target_test("target", target_json_test);
  data::InstallationResult result = OstreeManager::pull(config.pacman.sysroot, treehub_server, keys, target_test);

  EXPECT_EQ(result.result_code.num_code, data::ResultCode::Numeric::kInstallFailed);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }

  TemporaryDirectory temp_sysroot;
  test_sysroot = temp_sysroot / "sysroot";
  std::cout << "sysroot " << test_sysroot;
  // uses cp, as boost doesn't like to copy bad symlinks
  int r = system((std::string("cp -r ") + argv[1] + std::string(" ") + test_sysroot.string()).c_str());
  if (r != 0) {
    return -1;
  }

  std::string treehub_port = TestUtils::getFreePort();
  treehub_server += treehub_port;
  TemporaryDirectory treehub_dir;
  boost::process::child ostree_server_process("tests/sota_tools/treehub_server.py", std::string("-p"), treehub_port,
                                              std::string("-d"), treehub_dir.PathString(), std::string("--corrupt"),
                                              std::string("-s0.5"), std::string("--create"));
  TestUtils::waitForServer(treehub_server + "/");
  Process ostree("ostree");
  auto ostree_r = ostree.run({"rev-parse", std::string("--repo"), treehub_dir.PathString(), "master"});
  if (std::get<0>(ostree_r) != 0) {
    return -1;
  }
  new_rev = ostree.lastStdOut();
  boost::trim_if(new_rev, boost::is_any_of(" \t\r\n"));

  return RUN_ALL_TESTS();
}
#endif
