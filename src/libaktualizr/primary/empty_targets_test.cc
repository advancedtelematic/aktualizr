#include <gtest/gtest.h>

#include <string>

#include <boost/process.hpp>

#include "httpfake.h"
#include "primary/aktualizr.h"
#include "test_utils.h"
#include "uptane_test_common.h"

#include "utilities/fault_injection.h"

boost::filesystem::path aktualizr_repo_path;

class HttpRejectEmptyCorrId : public HttpFake {
 public:
  HttpRejectEmptyCorrId(const boost::filesystem::path &test_dir_in, const boost::filesystem::path &meta_dir_in)
      : HttpFake(test_dir_in, "", meta_dir_in) {}

  HttpResponse put(const std::string &url, const Json::Value &data) override {
    last_manifest = data;
    if (url.find("/manifest") != std::string::npos) {
      if (data["signed"].isMember("installation_report") &&
          data["signed"]["installation_report"]["report"]["correlation_id"].asString().empty()) {
        EXPECT_FALSE(data["signed"]["installation_report"]["report"]["correlation_id"].asString().empty());
        return HttpResponse(url, 400, CURLE_OK, "");
      }
    }
    return HttpResponse(url, 200, CURLE_OK, "");
  }
};

/*
 * Verify that we can successfully install an update after receiving
 * subsequent targets metadata that is empty.
 */
TEST(Aktualizr, EmptyTargets) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpRejectEmptyCorrId>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  conf.pacman.fake_need_reboot = true;
  conf.bootloader.reboot_sentinel_dir = temp_dir / "aktualizr-session";
  logger_set_threshold(boost::log::trivial::trace);

  Process akt_repo(aktualizr_repo_path.string());
  akt_repo.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  akt_repo.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt", "--targetname",
                "firmware.txt"});
  akt_repo.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                "--serial", "CA:FE:A6:D2:84:9D"});
  akt_repo.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  // Work around inconsistent directory naming.
  Utils::copyDir(meta_dir.Path() / "repo/image", meta_dir.Path() / "repo/repo");

  auto storage = INvStorage::newStorage(conf.storage);
  {
    Aktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    akt_repo.run({"emptytargets", "--path", meta_dir.PathString()});
    akt_repo.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

    result::UpdateCheck update_result2 = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result2.status, result::UpdateStatus::kUpdatesAvailable);

    result::Install install_result = aktualizr.Install(update_result2.updates).get();
    EXPECT_EQ(install_result.ecu_reports.size(), 1);
    EXPECT_EQ(install_result.ecu_reports[0].install_res.result_code.num_code,
              data::ResultCode::Numeric::kNeedCompletion);

    aktualizr.uptane_client_->package_manager_->completeInstall();
  }
  {
    Aktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    const Json::Value manifest = http->last_manifest["signed"];
    const Json::Value manifest_versions = manifest["ecu_version_manifests"];

    EXPECT_EQ(manifest["installation_report"]["report"]["items"].size(), 1);
    EXPECT_EQ(manifest["installation_report"]["report"]["items"][0]["ecu"].asString(), "CA:FE:A6:D2:84:9D");
    EXPECT_TRUE(manifest["installation_report"]["report"]["items"][0]["result"]["success"].asBool());

    EXPECT_EQ(manifest_versions["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"]["filepath"].asString(),
              "firmware.txt");
    EXPECT_EQ(manifest_versions["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 17);

    result::UpdateCheck update_result3 = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result3.status, result::UpdateStatus::kNoUpdatesAvailable);
  }
}

#ifdef FIU_ENABLE

/* Check that Aktualizr switches back to empty targets after completing an
 * installation attempt (OTA-2587) */
TEST(Aktualizr, EmptyTargetsAfterInstall) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpRejectEmptyCorrId>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process akt_repo(aktualizr_repo_path.string());
  akt_repo.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  akt_repo.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt", "--targetname",
                "firmware.txt"});
  akt_repo.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                "--serial", "CA:FE:A6:D2:84:9D"});
  akt_repo.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  // Work around inconsistent directory naming.
  Utils::copyDir(meta_dir.Path() / "repo/image", meta_dir.Path() / "repo/repo");

  // failing install
  auto storage = INvStorage::newStorage(conf.storage);
  {
    Aktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    update_result = aktualizr.CheckUpdates().get();
    fiu_init(0);
    fault_injection_enable("fake_package_install", 1, "", 0);
    result::Install install_result = aktualizr.Install(update_result.updates).get();
    EXPECT_EQ(install_result.ecu_reports.size(), 1);
    EXPECT_EQ(install_result.ecu_reports[0].install_res.result_code.num_code,
              data::ResultCode::Numeric::kInstallFailed);
    fault_injection_disable("fake_package_install");
  }

  // Backend reacts to failure: no need to install the target anymore
  akt_repo.run({"emptytargets", "--path", meta_dir.PathString()});
  akt_repo.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  // check that no update is available
  {
    Aktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);
  }
}

#endif

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the aktualizr-repo utility\n";
    return EXIT_FAILURE;
  }
  aktualizr_repo_path = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
