#include <gtest/gtest.h>

#include <boost/process.hpp>

#include "config/config.h"
#include "logging/logging.h"
#include "package_manager/ostreemanager.h"
#include "primary/aktualizr.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"
#include "uptane_test_common.h"

static std::string server = "http://127.0.0.1:";
static std::string treehub_server = "http://127.0.0.1:";
static boost::filesystem::path sysroot;

/*
 * Reject non-OSTree binaries.
 */
TEST(Aktualizr, DownloadNonOstreeBin) {
  TemporaryDirectory temp_dir;
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, server);
  conf.pacman.type = PACKAGE_MANAGER_OSTREE;
  conf.pacman.sysroot = sysroot.string();
  conf.pacman.ostree_server = treehub_server;
  conf.pacman.os = "dummy-os";
  conf.provision.device_id = "device_id";
  conf.provision.ecu_registration_endpoint = server + "/director/ecus";
  conf.tls.server = server;

  {
    std::shared_ptr<INvStorage> storage = INvStorage::newStorage(conf.storage);
    auto uptane_client = std_::make_unique<SotaUptaneClient>(conf, storage);
    uptane_client->initialize();
    EXPECT_FALSE(uptane_client->uptaneIteration(nullptr, nullptr));
    EXPECT_STREQ(uptane_client->getLastException().what(),
                 "The target had a non-OSTree package that can not be installed on an OSTree system.");
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();

  if (argc != 3) {
    std::cerr << "Error: " << argv[0] << " requires the path to the uptane-generator utility "
              << "and an OSTree sysroot\n";
    return EXIT_FAILURE;
  }

  boost::filesystem::path uptane_generator_path;
  uptane_generator_path = argv[1];

  TemporaryDirectory meta_dir;
  TemporaryDirectory temp_sysroot;
  sysroot = temp_sysroot / "sysroot";

  // uses cp, as boost doesn't like to copy bad symlinks
  int ret = system((std::string("cp -r ") + argv[2] + std::string(" ") + sysroot.string()).c_str());
  if (ret != 0) {
    return -1;
  }

  Process ostree("ostree");

  std::string new_rev = ostree.lastStdOut();
  boost::trim_if(new_rev, boost::is_any_of(" \t\r\n"));

  std::string port = TestUtils::getFreePort();
  server += port;
  boost::process::child http_server_process("tests/fake_http_server/fake_test_server.py", port, "-m", meta_dir.Path());
  TestUtils::waitForServer(server + "/");
  std::string treehub_port = TestUtils::getFreePort();
  treehub_server += treehub_port;
  TemporaryDirectory treehub_dir;
  boost::process::child ostree_server_process("tests/sota_tools/treehub_server.py", std::string("-p"), treehub_port,
                                              std::string("-d"), treehub_dir.PathString(), std::string("-s0.5"),
                                              std::string("--create"));
  TestUtils::waitForServer(treehub_server + "/");

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--targetname", "update_1.0", "--targetsha256", new_rev,
                  "--targetlength", "0", "--targetformat", "BINARY", "--hwid", "primary_hw"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "update_1.0", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  return RUN_ALL_TESTS();
}
#endif  // __NO_MAIN__
