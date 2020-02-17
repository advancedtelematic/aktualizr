#include <gtest/gtest.h>

#include <string>

#include "httpfake.h"
#include "primary/aktualizr.h"
#include "test_utils.h"
#include "uptane_test_common.h"
#include "utilities/fault_injection.h"

boost::filesystem::path uptane_generator_path;

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
  logger_set_threshold(boost::log::trivial::trace);

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  auto storage = INvStorage::newStorage(conf.storage);
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    uptane_gen.run({"emptytargets", "--path", meta_dir.PathString()});
    uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

    result::UpdateCheck update_result2 = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result2.status, result::UpdateStatus::kUpdatesAvailable);

    result::Install install_result = aktualizr.Install(update_result2.updates).get();
    ASSERT_EQ(install_result.ecu_reports.size(), 1);
    EXPECT_EQ(install_result.ecu_reports[0].install_res.result_code.num_code,
              data::ResultCode::Numeric::kNeedCompletion);

    aktualizr.uptane_client()->package_manager_->completeInstall();
  }
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
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

/* Check that Aktualizr switches back to empty targets after failing to verify
 * the Target matching. Also check that no updates are reported if any are
 * invalid. */
TEST(Aktualizr, EmptyTargetsAfterVerification) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpRejectEmptyCorrId>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  // Add two images: a valid one for the Primary and an invalid for the
  // Secondary. The Primary will get verified first and should succeed.
  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware_name.txt",
                  "--targetname", "firmware_name.txt", "--hwid", "bad"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware_name.txt", "--hwid",
                  "secondary_hw", "--serial", "secondary_ecu_serial"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  // failing verification
  auto storage = INvStorage::newStorage(conf.storage);
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kError);
    EXPECT_EQ(update_result.ecus_count, 0);
    EXPECT_TRUE(update_result.updates.empty());
  }

  // Backend reacts to failure: no need to install the target anymore
  uptane_gen.run({"emptytargets", "--path", meta_dir.PathString()});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  // check that no update is available
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);
  }
}

#ifdef FIU_ENABLE

/* Check that Aktualizr switches back to empty targets after failing a
 * download attempt (OTA-3169) */
TEST(Aktualizr, EmptyTargetsAfterDownload) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpRejectEmptyCorrId>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  // failing download
  fault_injection_init();
  auto storage = INvStorage::newStorage(conf.storage);
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    fault_injection_enable("fake_package_download", 1, "", 0);
    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kError);
    fault_injection_disable("fake_package_download");
  }

  // Backend reacts to failure: no need to install the target anymore
  uptane_gen.run({"emptytargets", "--path", meta_dir.PathString()});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  // check that no update is available
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);
  }
}

/* Check that Aktualizr switches back to empty targets after failing an
 * installation attempt (OTA-2587) */
TEST(Aktualizr, EmptyTargetsAfterInstall) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  auto http = std::make_shared<HttpRejectEmptyCorrId>(temp_dir.Path(), meta_dir.Path() / "repo");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  logger_set_threshold(boost::log::trivial::trace);

  Process uptane_gen(uptane_generator_path.string());
  uptane_gen.run({"generate", "--path", meta_dir.PathString(), "--correlationid", "abc123"});
  uptane_gen.run({"image", "--path", meta_dir.PathString(), "--filename", "tests/test_data/firmware.txt",
                  "--targetname", "firmware.txt", "--hwid", "primary_hw"});
  uptane_gen.run({"addtarget", "--path", meta_dir.PathString(), "--targetname", "firmware.txt", "--hwid", "primary_hw",
                  "--serial", "CA:FE:A6:D2:84:9D"});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  // failing install
  fault_injection_init();
  auto storage = INvStorage::newStorage(conf.storage);
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    fault_injection_enable("fake_package_install", 1, "", 0);
    result::Install install_result = aktualizr.Install(download_result.updates).get();
    EXPECT_EQ(install_result.ecu_reports.size(), 1);
    EXPECT_EQ(install_result.ecu_reports[0].install_res.result_code.num_code,
              data::ResultCode::Numeric::kInstallFailed);
    fault_injection_disable("fake_package_install");
  }

  // Backend reacts to failure: no need to install the target anymore
  uptane_gen.run({"emptytargets", "--path", meta_dir.PathString()});
  uptane_gen.run({"signtargets", "--path", meta_dir.PathString(), "--correlationid", "abc123"});

  // check that no update is available
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
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
    std::cerr << "Error: " << argv[0] << " requires the path to the uptane-generator utility\n";
    return EXIT_FAILURE;
  }
  uptane_generator_path = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
