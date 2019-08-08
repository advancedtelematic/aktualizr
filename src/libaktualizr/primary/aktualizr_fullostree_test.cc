#include <gtest/gtest.h>

#include <future>
#include <iostream>
#include <string>
#include <thread>

#include <boost/process.hpp>

#include "uptane_test_common.h"

#include "config/config.h"
#include "logging/logging.h"
#include "package_manager/ostreemanager.h"
#include "primary/aktualizr.h"
#include "storage/sqlstorage.h"
#include "test_utils.h"

boost::filesystem::path aktualizr_repo_path;
static std::string server = "http://127.0.0.1:";
static std::string treehub_server = "http://127.0.0.1:";
static boost::filesystem::path sysroot;

static struct {
  int serial{0};
  std::string rev;
} ostree_deployment;
static std::string new_rev;

#include <ostree.h>
extern "C" OstreeDeployment *ostree_sysroot_get_booted_deployment(OstreeSysroot *self) {
  (void)self;
  static GObjectUniquePtr<OstreeDeployment> dep;

  dep.reset(ostree_deployment_new(0, "dummy-os", ostree_deployment.rev.c_str(), ostree_deployment.serial,
                                  ostree_deployment.rev.c_str(), ostree_deployment.serial));
  return dep.get();
}

extern "C" const char *ostree_deployment_get_csum(OstreeDeployment *self) {
  (void)self;
  return ostree_deployment.rev.c_str();
}

/*
 * Install an OSTree update on the primary.
 */
TEST(Aktualizr, FullOstreeUpdate) {
  TemporaryDirectory temp_dir;
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, server);
  conf.pacman.type = PackageManager::kOstree;
  conf.pacman.sysroot = sysroot.string();
  conf.pacman.ostree_server = treehub_server;
  conf.pacman.os = "dummy-os";
  conf.provision.device_id = "device_id";
  conf.provision.ecu_registration_endpoint = server + "/director/ecus";
  conf.tls.server = server;

  LOG_INFO << "conf: " << conf;

  {
    Aktualizr aktualizr(conf);

    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    ASSERT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    result::Install install_result = aktualizr.Install(update_result.updates).get();
    EXPECT_EQ(install_result.ecu_reports.size(), 1);
    EXPECT_EQ(install_result.ecu_reports[0].install_res.result_code.num_code,
              data::ResultCode::Numeric::kNeedCompletion);
  }

  // do "reboot" and finalize
  ostree_deployment.serial = 1;
  ostree_deployment.rev = new_rev;
  boost::filesystem::remove(conf.bootloader.reboot_sentinel_dir / conf.bootloader.reboot_sentinel_name);

  {
    UptaneTestCommon::TestAktualizr aktualizr(conf);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    ASSERT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);

    // check new version
    const auto target = aktualizr.uptane_client()->package_manager_->getCurrent();
    EXPECT_EQ(target.sha256Hash(), new_rev);
  }
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();

  if (argc != 3) {
    std::cerr << "Error: " << argv[0] << " requires the path to the aktualizr-repo utility "
              << "and an OStree sysroot\n";
    return EXIT_FAILURE;
  }
  aktualizr_repo_path = argv[1];

  Process ostree("ostree");

  TemporaryDirectory meta_dir;
  TemporaryDirectory temp_sysroot;
  sysroot = temp_sysroot / "sysroot";
  // uses cp, as boost doesn't like to copy bad symlinks
  int res = system((std::string("cp -r ") + argv[2] + std::string(" ") + sysroot.string()).c_str());
  if (res != 0) {
    return -1;
  }
  auto r = ostree.run(
      {"rev-parse", std::string("--repo"), (sysroot / "/ostree/repo").string(), "generate-remote/generated"});
  if (std::get<0>(r) != 0) {
    return -1;
  }
  ostree_deployment.serial = 0;
  ostree_deployment.rev = ostree.lastStdOut();
  boost::trim_if(ostree_deployment.rev, boost::is_any_of(" \t\r\n"));
  LOG_INFO << "ORIG: " << ostree_deployment.rev;

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
  r = ostree.run({"rev-parse", std::string("--repo"), treehub_dir.PathString(), "master"});
  if (std::get<0>(r) != 0) {
    return -1;
  }
  new_rev = ostree.lastStdOut();
  boost::trim_if(new_rev, boost::is_any_of(" \t\r\n"));
  LOG_INFO << "DEST: " << new_rev;

  Process akt_repo(aktualizr_repo_path.string());
  akt_repo.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  akt_repo.run({"image", "--path", meta_dir.PathString(), "--targetname", "update_1.0", "--targetsha256", new_rev,
                "--targetlength", "0", "--targetformat", "OSTREE", "--hwid", "primary_hw"});
  akt_repo.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "update_1.0", "--hwid", "primary_hw",
                "--serial", "CA:FE:A6:D2:84:9D"});
  akt_repo.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  LOG_INFO << akt_repo.lastStdOut();
  // Work around inconsistent directory naming.
  Utils::copyDir(meta_dir.Path() / "repo/image", meta_dir.Path() / "repo/repo");

  return RUN_ALL_TESTS();
}
#endif  // __NO_MAIN__
