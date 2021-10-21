#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "libaktualizr/aktualizr.h"
#include "libaktualizr/config.h"
#include "libaktualizr/events.h"

#include "httpfake.h"
#include "primary/aktualizr_helpers.h"
#include "primary/sotauptaneclient.h"
#include "uptane_test_common.h"
#include "utilities/utils.h"
#include "virtualsecondary.h"

#include "utilities/fault_injection.h"

boost::filesystem::path uptane_repos_dir;
boost::filesystem::path fake_meta_dir;

void verifyNothingInstalled(const Json::Value& manifest) {
  // Verify nothing has installed for the Primary.
  EXPECT_EQ(
      manifest["ecu_version_manifests"]["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["id"].asString(),
      "");
  EXPECT_EQ(
      manifest["ecu_version_manifests"]["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_code"]
          .asInt(),
      static_cast<int>(data::ResultCode::Numeric::kOk));
  EXPECT_EQ(
      manifest["ecu_version_manifests"]["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_text"]
          .asString(),
      "");
  EXPECT_EQ(manifest["ecu_version_manifests"]["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"]["filepath"].asString(),
            "unknown");
  // Verify nothing has installed for the Secondary.
  EXPECT_EQ(
      manifest["ecu_version_manifests"]["secondary_ecu_serial"]["signed"]["installed_image"]["filepath"].asString(),
      "noimage");
}

/*
 * Initialize -> UptaneCycle -> no updates -> no further action or events.
 */
TEST(Aktualizr, FullNoUpdates) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  struct {
    size_t num_events{0};
    std::future<void> future;
    std::promise<void> promise;
  } ev_state;
  ev_state.future = ev_state.promise.get_future();

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    if (event->isTypeOf<event::DownloadProgressReport>()) {
      return;
    }
    LOG_INFO << "Got " << event->variant;
    switch (ev_state.num_events) {
      case 0: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 0);
        EXPECT_EQ(targets_event->result.updates.size(), 0);
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
        break;
      }
      case 1: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 0);
        EXPECT_EQ(targets_event->result.updates.size(), 0);
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
        ev_state.promise.set_value();
        break;
      }
      case 5:
        // Don't let the test run indefinitely!
        FAIL() << "Unexpected events!";
      default:
        std::cout << "event #" << ev_state.num_events << " is: " << event->variant << "\n";
        ASSERT_EQ(event->variant, "");
    }
    ++ev_state.num_events;
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();
  // Run the cycle twice so that we can check for a second UpdateCheckComplete
  // and guarantee that nothing unexpected happened after the first fetch.
  aktualizr.UptaneCycle();
  auto status = ev_state.future.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(aktualizr.uptane_client()->AssembleManifest());
}

/*
 * Compute device installation failure code as concatenation of ECU failure
 * codes during installation.
 */
TEST(Aktualizr, DeviceInstallationResult) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  EcuSerials serials{
      {Uptane::EcuSerial("CA:FE:A6:D2:84:9D"), Uptane::HardwareIdentifier("primary_hw")},
      {Uptane::EcuSerial("secondary_ecu_serial"), Uptane::HardwareIdentifier("secondary_hw")},
      {Uptane::EcuSerial("ecuserial3"), Uptane::HardwareIdentifier("hw_id3")},
  };
  storage->storeEcuSerials(serials);

  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  Primary::VirtualSecondaryConfig ecu_config = UptaneTestCommon::altVirtualConfiguration(temp_dir.Path());
  aktualizr.AddSecondary(std::make_shared<Primary::VirtualSecondary>(ecu_config));
  aktualizr.Initialize();

  storage->saveEcuInstallationResult(Uptane::EcuSerial("ecuserial3"), data::InstallationResult());
  storage->saveEcuInstallationResult(Uptane::EcuSerial("CA:FE:A6:D2:84:9D"), data::InstallationResult());
  storage->saveEcuInstallationResult(Uptane::EcuSerial("CA:FE:A6:D2:84:9D"),
                                     data::InstallationResult(data::ResultCode::Numeric::kInstallFailed, ""));

  data::InstallationResult result;
  aktualizr.uptane_client()->computeDeviceInstallationResult(&result, nullptr);
  auto res_json = result.toJson();
  EXPECT_EQ(res_json["code"].asString(), "primary_hw:INSTALL_FAILED");
  EXPECT_EQ(res_json["success"], false);

  storage->saveEcuInstallationResult(
      Uptane::EcuSerial("ecuserial3"),
      data::InstallationResult(data::ResultCode(data::ResultCode::Numeric::kInstallFailed, "SECOND_FAIL"), ""));
  aktualizr.uptane_client()->computeDeviceInstallationResult(&result, nullptr);
  res_json = result.toJson();
  EXPECT_EQ(res_json["code"].asString(), "primary_hw:INSTALL_FAILED|hw_id3:SECOND_FAIL");
  EXPECT_EQ(res_json["success"], false);
}

#ifdef FIU_ENABLE

/*
 * Compute device installation failure code as concatenation of ECU failure
 * codes from sending metadata to Secondaries.
 */
TEST(Aktualizr, DeviceInstallationResultMetadata) {
  TemporaryDirectory temp_dir;
  // Use "hasupdates" to make sure Image repo Root gets fetched, despite that we
  // won't use the default update.
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "sec_serial1", "sec_hw1");
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "sec_serial2", "sec_hw2");
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "sec_serial3", "sec_hw3");

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();
  auto update_result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

  fault_injection_init();
  fiu_enable("secondary_putmetadata", 1, nullptr, 0);

  // Try updating two Secondaries; leave the third one alone.
  std::vector<Uptane::Target> targets;
  Json::Value target_json;
  target_json["custom"]["targetFormat"] = "BINARY";
  target_json["custom"]["ecuIdentifiers"]["sec_serial1"]["hardwareId"] = "sec_hw1";
  target_json["custom"]["ecuIdentifiers"]["sec_serial3"]["hardwareId"] = "sec_hw3";
  targets.emplace_back(Uptane::Target("test", target_json));

  data::InstallationResult result;
  aktualizr.uptane_client()->sendMetadataToEcus(targets, &result, nullptr);
  auto res_json = result.toJson();
  EXPECT_EQ(res_json["code"].asString(), "sec_hw1:VERIFICATION_FAILED|sec_hw3:VERIFICATION_FAILED");
  EXPECT_EQ(res_json["success"], false);

  fiu_disable("secondary_putmetadata");

  // Retry after disabling fault injection to verify the test.
  aktualizr.uptane_client()->sendMetadataToEcus(targets, &result, nullptr);
  res_json = result.toJson();
  EXPECT_EQ(res_json["code"].asString(), "OK");
  EXPECT_EQ(res_json["success"], true);
}

#endif  // FIU_ENABLE

class HttpFakeEventCounter : public HttpFake {
 public:
  HttpFakeEventCounter(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "hasupdates", meta_dir_in) {}

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    for (const Json::Value& event : data) {
      ++events_seen;
      std::string event_type = event["eventType"]["id"].asString();
      if (event_type.find("Ecu") == 0) {
        EXPECT_EQ(event["event"]["correlationId"], "id0");
      }

      std::cout << "got event #" << events_seen << ": " << event_type << "\n";
      if (events_seen >= 1 && events_seen <= 4) {
        EXPECT_TRUE(event_type == "EcuDownloadStarted" || event_type == "EcuDownloadCompleted");
      } else if (events_seen >= 5 && events_seen <= 8) {
        EXPECT_TRUE(event_type == "EcuInstallationStarted" || event_type == "EcuInstallationCompleted");
      } else {
        std::cout << "Unexpected event";
        EXPECT_EQ(0, 1);
      }
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  unsigned int events_seen{0};
};

/*
 * Initialize -> UptaneCycle -> updates downloaded and installed for Primary and
 * Secondary.
 */
TEST(Aktualizr, FullWithUpdates) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeEventCounter>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  struct {
    size_t num_events{0};
    std::future<void> future;
    std::promise<void> promise;
  } ev_state;
  ev_state.future = ev_state.promise.get_future();

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    if (event->isTypeOf<event::DownloadProgressReport>()) {
      return;
    }
    LOG_INFO << "Got " << event->variant;
    switch (ev_state.num_events) {
      case 0: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 2);
        EXPECT_EQ(targets_event->result.updates.size(), 2u);
        EXPECT_EQ(targets_event->result.updates[0].filename(), "primary_firmware.txt");
        EXPECT_EQ(targets_event->result.updates[1].filename(), "secondary_firmware.txt");
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kUpdatesAvailable);
        break;
      }
      case 1:
      case 2: {
        ASSERT_EQ(event->variant, "DownloadTargetComplete");
        const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
        EXPECT_TRUE(download_event->update.filename() == "primary_firmware.txt" ||
                    download_event->update.filename() == "secondary_firmware.txt");
        EXPECT_TRUE(download_event->success);
        break;
      }
      case 3: {
        ASSERT_EQ(event->variant, "AllDownloadsComplete");
        const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
        EXPECT_EQ(downloads_complete->result.updates.size(), 2);
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "primary_firmware.txt" ||
                    downloads_complete->result.updates[1].filename() == "primary_firmware.txt");
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "secondary_firmware.txt" ||
                    downloads_complete->result.updates[1].filename() == "secondary_firmware.txt");
        EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kSuccess);
        break;
      }
      case 4: {
        // Primary always gets installed first. (Not a requirement, just how it
        // works at present.)
        ASSERT_EQ(event->variant, "InstallStarted");
        const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
        EXPECT_EQ(install_started->serial.ToString(), "CA:FE:A6:D2:84:9D");
        break;
      }
      case 5: {
        // Primary should complete before Secondary begins. (Again not a
        // requirement per se.)
        ASSERT_EQ(event->variant, "InstallTargetComplete");
        const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
        EXPECT_EQ(install_complete->serial.ToString(), "CA:FE:A6:D2:84:9D");
        EXPECT_TRUE(install_complete->success);
        break;
      }
      case 6: {
        ASSERT_EQ(event->variant, "InstallStarted");
        const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
        EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
        break;
      }
      case 7: {
        ASSERT_EQ(event->variant, "InstallTargetComplete");
        const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
        EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
        EXPECT_TRUE(install_complete->success);
        break;
      }
      case 8: {
        ASSERT_EQ(event->variant, "AllInstallsComplete");
        const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
        EXPECT_EQ(installs_complete->result.ecu_reports.size(), 2);
        EXPECT_EQ(installs_complete->result.ecu_reports[0].install_res.result_code.num_code,
                  data::ResultCode::Numeric::kOk);
        EXPECT_EQ(installs_complete->result.ecu_reports[1].install_res.result_code.num_code,
                  data::ResultCode::Numeric::kOk);
        break;
      }
      case 9: {
        ASSERT_EQ(event->variant, "PutManifestComplete");
        const auto put_complete = dynamic_cast<event::PutManifestComplete*>(event.get());
        EXPECT_TRUE(put_complete->success);
        ev_state.promise.set_value();
        break;
      }
      case 12:
        // Don't let the test run indefinitely!
        FAIL() << "Unexpected events!";
      default:
        std::cout << "event #" << ev_state.num_events << " is: " << event->variant << "\n";
        ASSERT_EQ(event->variant, "");
    }
    ++ev_state.num_events;
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();
  auto status = ev_state.future.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }
  EXPECT_EQ(http->events_seen, 8);
}

class HttpFakeSplit : public HttpFake {
 public:
  HttpFakeSplit(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "hasupdates", meta_dir_in) {}

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    for (const Json::Value& event : data) {
      ++events_seen;
      std::string event_type = event["eventType"]["id"].asString();
      if (event_type.find("Ecu") == 0) {
        EXPECT_EQ(event["event"]["correlationId"], "id0");
      }

      std::cout << "got event #" << events_seen << ": " << event_type << "\n";
      switch (events_seen) {
        case 1:
        case 5:
          EXPECT_TRUE(event_type == "EcuDownloadStarted");
          break;
        case 2:
        case 6:
          EXPECT_TRUE(event_type == "EcuDownloadCompleted");
          break;
        case 3:
        case 7:
          EXPECT_TRUE(event_type == "EcuInstallationStarted");
          break;
        case 4:
        case 8:
          EXPECT_TRUE(event_type == "EcuInstallationCompleted");
          break;
        default:
          std::cout << "Unexpected event";
          EXPECT_EQ(0, 1);
          break;
      }
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  unsigned int events_seen{0};
};

/*
 * Initialize -> CheckUpdates -> download and install one update -> download and
 * install second update
 *
 * This is intended to cover the case where an implementer splits an update with
 * multiple targets and downloads and/or installs them separately. This is
 * supported as long as a check for updates isn't done inbetween. It was briefly
 * broken by overzealously dropping Targets metadata and fixed in
 * 99c7f7ef20da76a2d6eefd08be5529c36434b9a6.
 */
TEST(Aktualizr, SplitUpdates) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeSplit>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  struct {
    size_t num_events{0};
    std::future<void> future;
    std::promise<void> promise;
  } ev_state;
  ev_state.future = ev_state.promise.get_future();

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    if (event->isTypeOf<event::DownloadProgressReport>()) {
      return;
    }
    LOG_INFO << "Got " << event->variant;
    switch (ev_state.num_events) {
      case 0: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 2);
        EXPECT_EQ(targets_event->result.updates.size(), 2u);
        EXPECT_EQ(targets_event->result.updates[0].filename(), "primary_firmware.txt");
        EXPECT_EQ(targets_event->result.updates[1].filename(), "secondary_firmware.txt");
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kUpdatesAvailable);
        break;
      }
      case 1: {
        ASSERT_EQ(event->variant, "DownloadTargetComplete");
        const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
        EXPECT_TRUE(download_event->update.filename() == "primary_firmware.txt");
        EXPECT_TRUE(download_event->success);
        break;
      }
      case 2: {
        ASSERT_EQ(event->variant, "AllDownloadsComplete");
        const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
        EXPECT_EQ(downloads_complete->result.updates.size(), 1);
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "primary_firmware.txt");
        EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kSuccess);
        break;
      }
      case 3: {
        // Primary always gets installed first. (Not a requirement, just how it
        // works at present.)
        ASSERT_EQ(event->variant, "InstallStarted");
        const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
        EXPECT_EQ(install_started->serial.ToString(), "CA:FE:A6:D2:84:9D");
        break;
      }
      case 4: {
        // Primary should complete before Secondary begins. (Again not a
        // requirement per se.)
        ASSERT_EQ(event->variant, "InstallTargetComplete");
        const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
        EXPECT_EQ(install_complete->serial.ToString(), "CA:FE:A6:D2:84:9D");
        EXPECT_TRUE(install_complete->success);
        break;
      }
      case 5: {
        ASSERT_EQ(event->variant, "AllInstallsComplete");
        const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
        EXPECT_EQ(installs_complete->result.ecu_reports.size(), 1);
        EXPECT_EQ(installs_complete->result.ecu_reports[0].install_res.result_code.num_code,
                  data::ResultCode::Numeric::kOk);
        break;
      }
      case 6: {
        ASSERT_EQ(event->variant, "DownloadTargetComplete");
        const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
        EXPECT_TRUE(download_event->update.filename() == "secondary_firmware.txt");
        EXPECT_TRUE(download_event->success);
        break;
      }
      case 7: {
        ASSERT_EQ(event->variant, "AllDownloadsComplete");
        const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
        EXPECT_EQ(downloads_complete->result.updates.size(), 1);
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "secondary_firmware.txt");
        EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kSuccess);
        break;
      }
      case 8: {
        ASSERT_EQ(event->variant, "InstallStarted");
        const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
        EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
        break;
      }
      case 9: {
        ASSERT_EQ(event->variant, "InstallTargetComplete");
        const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
        EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
        EXPECT_TRUE(install_complete->success);
        break;
      }
      case 10: {
        ASSERT_EQ(event->variant, "AllInstallsComplete");
        const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
        EXPECT_EQ(installs_complete->result.ecu_reports.size(), 1);
        EXPECT_EQ(installs_complete->result.ecu_reports[0].install_res.result_code.num_code,
                  data::ResultCode::Numeric::kOk);
        break;
      }
      case 11: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 0);
        EXPECT_EQ(targets_event->result.updates.size(), 0);
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
        ev_state.promise.set_value();
        break;
      }
      case 15:
        // Don't let the test run indefinitely!
        FAIL() << "Unexpected events!";
      default:
        std::cout << "event #" << ev_state.num_events << " is: " << event->variant << "\n";
        ASSERT_EQ(event->variant, "");
    }
    ++ev_state.num_events;
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  ASSERT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  ASSERT_EQ(update_result.ecus_count, 2);

  // Try to install the updates in two separate calls.
  result::Download download_result = aktualizr.Download({update_result.updates[0]}).get();
  ASSERT_EQ(download_result.status, result::DownloadStatus::kSuccess);
  aktualizr.Install(download_result.updates);

  download_result = aktualizr.Download({update_result.updates[1]}).get();
  ASSERT_EQ(download_result.status, result::DownloadStatus::kSuccess);
  aktualizr.Install(download_result.updates);

  update_result = aktualizr.CheckUpdates().get();
  ASSERT_EQ(update_result.status, result::UpdateStatus::kNoUpdatesAvailable);

  auto status = ev_state.future.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }
  EXPECT_EQ(http->events_seen, 8);
}

class HttpFakePutCounter : public HttpFake {
 public:
  HttpFakePutCounter(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "hasupdates", meta_dir_in) {}

  HttpResponse put(const std::string& url, const Json::Value& data) override {
    if (url.find("/manifest") != std::string::npos) {
      manifest_sends += 1;
    }
    return HttpFake::put(url, data);
  }

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    // store all events in an array for later inspection
    for (const Json::Value& event : data) {
      events.push_back(event);
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  size_t count_event_with_type(const std::string& event_type) {
    auto c = std::count_if(events.cbegin(), events.cend(), [&event_type](const Json::Value& v) {
      return v["eventType"]["id"].asString() == event_type;
    });
    return static_cast<size_t>(c);
  }

  unsigned int manifest_sends{0};
  std::vector<Json::Value> events;
};

/*
 * Initialize -> UptaneCycle -> updates downloaded and installed for Primary
 * (after reboot) and Secondary (aktualizr_test.cc)
 *
 * It simulates closely the OSTree case which needs a reboot after applying an
 * update, but uses `PackageManagerFake`.
 *
 * Checks actions:
 *
 * - [x] Finalize a pending update that requires reboot
 *   - [x] Store installation result
 *   - [x] Send manifest
 *   - [x] Update is not in pending state anymore after successful finalization
 *
 *  - [x] Install an update on the Primary
 *    - [x] Set new version to pending status after an OSTree update trigger
 *    - [x] Send EcuInstallationAppliedReport to server after an OSTree update trigger
 *    - [x] Uptane check for updates and manifest sends are disabled while an installation
 *          is pending reboot
 */
TEST(Aktualizr, FullWithUpdatesNeedReboot) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakePutCounter>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  conf.pacman.fake_need_reboot = true;

  {
    // first run: do the install
    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();
    aktualizr.UptaneCycle();

    // check that a version is here, set to pending

    boost::optional<Uptane::Target> pending_target;
    storage->loadPrimaryInstalledVersions(nullptr, &pending_target);
    EXPECT_TRUE(!!pending_target);
  }

  // check that no manifest has been sent after the update application
  EXPECT_EQ(http->manifest_sends, 1);
  EXPECT_EQ(http->count_event_with_type("EcuInstallationStarted"), 2);    // two ECUs have started
  EXPECT_EQ(http->count_event_with_type("EcuInstallationApplied"), 1);    // Primary ECU has been applied
  EXPECT_EQ(http->count_event_with_type("EcuInstallationCompleted"), 1);  // Secondary ECU has been updated

  {
    // second run: before reboot, re-use the storage
    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    // check that everything is still pending
    boost::optional<Uptane::Target> pending_target;
    storage->loadPrimaryInstalledVersions(nullptr, &pending_target);
    EXPECT_TRUE(!!pending_target);

    result::UpdateCheck update_res = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_res.status, result::UpdateStatus::kError);

    // simulate a reboot
    boost::filesystem::remove(conf.bootloader.reboot_sentinel_dir / conf.bootloader.reboot_sentinel_name);
  }

  // still no manifest
  EXPECT_EQ(http->manifest_sends, 1);
  EXPECT_EQ(http->count_event_with_type("EcuInstallationCompleted"), 1);  // no more installation completed

  {
    // third run: after reboot, re-use the storage
    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.Initialize();

    result::UpdateCheck update_res = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_res.status, result::UpdateStatus::kNoUpdatesAvailable);

    // Primary is installed, nothing pending
    boost::optional<Uptane::Target> current_target;
    boost::optional<Uptane::Target> pending_target;
    storage->loadPrimaryInstalledVersions(&current_target, &pending_target);
    EXPECT_TRUE(!!current_target);
    EXPECT_FALSE(!!pending_target);

    // Secondary is installed, nothing pending
    boost::optional<Uptane::Target> sec_current_target;
    boost::optional<Uptane::Target> sec_pending_target;
    storage->loadInstalledVersions("secondary_ecu_serial", &sec_current_target, &sec_pending_target);
    EXPECT_TRUE(!!sec_current_target);
    EXPECT_FALSE(!!sec_pending_target);
  }

  // check that the manifest has been sent
  EXPECT_EQ(http->manifest_sends, 3);
  EXPECT_EQ(http->count_event_with_type("EcuInstallationCompleted"), 2);  // 2 installations completed

  // check good correlation ids
  for (const Json::Value& event : http->events) {
    std::cout << "got event " << event["eventType"]["id"].asString() << "\n";
    EXPECT_EQ(event["event"]["correlationId"].asString(), "id0");
  }
}

#ifdef FIU_ENABLE

class HttpInstallationFailed : public HttpFake {
 public:
  HttpInstallationFailed(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "hasupdates", meta_dir_in) {}

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;

    for (auto& event_data : data) {
      auto event_id = event_data["eventType"]["id"].asString();
      auto ecu = event_data["event"]["ecu"].asString();
      auto correlationID = event_data["event"]["correlationId"].asString();

      if (event_id == "EcuInstallationCompleted") {
        install_completion_status_[ecu] = event_data["event"]["success"].asBool();
      }
      report_events_.push_back(event_id);
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  bool checkReceivedReports(const std::vector<std::string>& expected_event_order) {
    bool result = true;
    auto received_event_it = report_events_.begin();

    for (auto& expected_event : expected_event_order) {
      auto received_event = *received_event_it;
      if (expected_event != received_event) {
        result = false;
        break;
      }
      ++received_event_it;
    }
    return result;
  }

  bool wasInstallSuccessful(const std::string& ecu_serial) const {
    auto find_it = install_completion_status_.find(ecu_serial);
    if (find_it == install_completion_status_.end()) {
      return false;
    } else {
      return (*find_it).second;
    }
  }
  void clearReportEvents() { report_events_.clear(); }

 private:
  std::vector<std::string> report_events_;
  std::unordered_map<std::string, bool> install_completion_status_;
};

class EventHandler {
 public:
  EventHandler(Aktualizr& aktualizr) {
    functor_ = std::bind(&EventHandler::operator(), this, std::placeholders::_1);
    aktualizr.SetSignalHandler(functor_);
  }

  void operator()(const std::shared_ptr<event::BaseEvent>& event) {
    ASSERT_NE(event, nullptr);
    received_events_.push_back(event->variant);
  }

  bool checkReceivedEvents(const std::vector<std::string>& expected_event_order) {
    bool result = true;
    auto received_event_it = received_events_.begin();

    for (auto& expected_event : expected_event_order) {
      auto received_event = *received_event_it;
      if (expected_event != received_event) {
        result = false;
        break;
      }

      if (received_event == "DownloadProgressReport") {
        while (*(++received_event_it) == "DownloadProgressReport") {
        }
      } else {
        ++received_event_it;
      }
    }
    return result;
  }

 private:
  std::function<void(std::shared_ptr<event::BaseEvent>)> functor_;
  std::vector<std::string> received_events_{};
};

/*
 * Initialize -> UptaneCycle -> download updates and install them ->
 * -> reboot emulated -> Initialize -> Fail installation finalization
 *
 * Verifies whether the Uptane client is not at pending state after installation finalization failure
 *
 * Checks actions:
 *
 * - [x] Update is not in pending state anymore after failed finalization
 */
TEST(Aktualizr, FinalizationFailure) {
  TemporaryDirectory temp_dir;
  auto http_server_mock = std::make_shared<HttpInstallationFailed>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http_server_mock->tls_server);
  conf.pacman.fake_need_reboot = true;
  conf.uptane.force_install_completion = true;
  conf.uptane.polling_sec = 0;
  auto storage = INvStorage::newStorage(conf.storage);

  std::vector<std::string> expected_event_order = {
      "SendDeviceDataComplete", "UpdateCheckComplete",    "DownloadProgressReport", "DownloadTargetComplete",
      "DownloadProgressReport", "DownloadTargetComplete", "AllDownloadsComplete",   "InstallStarted",
      "InstallTargetComplete",  "InstallStarted",         "InstallTargetComplete",  "AllInstallsComplete"};

  std::vector<std::string> expected_report_order = {
      "EcuDownloadStarted",     "EcuDownloadCompleted",   "EcuDownloadStarted",     "EcuDownloadCompleted",
      "EcuInstallationStarted", "EcuInstallationApplied", "EcuInstallationStarted", "EcuInstallationCompleted"};

  const std::string primary_ecu_id = "CA:FE:A6:D2:84:9D";
  const std::string secondary_ecu_id = "secondary_ecu_serial";

  {
    // get update, install them and emulate reboot
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http_server_mock);
    EventHandler event_hdlr(aktualizr);
    aktualizr.Initialize();

    // verify currently installed version
    boost::optional<Uptane::Target> current_version;
    boost::optional<Uptane::Target> pending_version;
    ASSERT_TRUE(storage->loadInstalledVersions(primary_ecu_id, &current_version, &pending_version));

    // for some reason there is no any installed version at initial Aktualizr boot/run
    // IMHO it should return currently installed version
    EXPECT_FALSE(!!pending_version);
    EXPECT_FALSE(!!current_version);

    auto aktualizr_cycle_thread = aktualizr.RunForever();
    auto aktualizr_cycle_thread_status = aktualizr_cycle_thread.wait_for(std::chrono::seconds(20));

    ASSERT_EQ(aktualizr_cycle_thread_status, std::future_status::ready);
    EXPECT_TRUE(aktualizr.uptane_client()->isInstallCompletionRequired());
    EXPECT_TRUE(event_hdlr.checkReceivedEvents(expected_event_order));
    EXPECT_TRUE(aktualizr.uptane_client()->hasPendingUpdates());
    EXPECT_TRUE(http_server_mock->checkReceivedReports(expected_report_order));
    // Aktualizr reports to a server that installation was successfull for the Secondary
    // checkReceivedReports() verifies whether EcuInstallationApplied was reported for the Primary
    EXPECT_FALSE(http_server_mock->wasInstallSuccessful(primary_ecu_id));
    EXPECT_TRUE(http_server_mock->wasInstallSuccessful(secondary_ecu_id));

    data::InstallationResult dev_installation_res;
    std::string report;
    std::string correlation_id;
    ASSERT_TRUE(storage->loadDeviceInstallationResult(&dev_installation_res, &report, &correlation_id));
    EXPECT_EQ(dev_installation_res.result_code.num_code, data::ResultCode::Numeric::kNeedCompletion);

    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_installation_res;
    ASSERT_TRUE(storage->loadEcuInstallationResults(&ecu_installation_res));
    EXPECT_EQ(ecu_installation_res.size(), 2);

    for (const auto& ecu_install_res : ecu_installation_res) {
      auto ecu_id = ecu_install_res.first.ToString();

      if (ecu_id == primary_ecu_id) {
        EXPECT_EQ(ecu_install_res.second.result_code.num_code, data::ResultCode::Numeric::kNeedCompletion);
        EXPECT_EQ(ecu_install_res.second.success, false);
        EXPECT_EQ(ecu_install_res.second.description, "Application successful, need reboot");

      } else if (ecu_id == secondary_ecu_id) {
        EXPECT_EQ(ecu_install_res.second.result_code.num_code, data::ResultCode::Numeric::kOk);
        EXPECT_EQ(ecu_install_res.second.success, true);
        EXPECT_EQ(ecu_install_res.second.description, "");

      } else {
        FAIL() << "Unexpected ECU serial: " << ecu_install_res.first;
      }
    }

    pending_version = boost::none;
    current_version = boost::none;

    ASSERT_TRUE(storage->loadInstalledVersions(primary_ecu_id, &current_version, &pending_version));
    EXPECT_FALSE(!!current_version);
    EXPECT_TRUE(!!pending_version);
    EXPECT_TRUE(pending_version->IsValid());

    pending_version = boost::none;
    current_version = boost::none;

    ASSERT_TRUE(storage->loadInstalledVersions(secondary_ecu_id, &current_version, &pending_version));
    EXPECT_TRUE(!!current_version);
    EXPECT_TRUE(current_version->IsValid());
    EXPECT_FALSE(!!pending_version);
  }

  {
    // now try to finalize the previously installed updates with enabled failure injection
    fault_injection_init();
    fiu_enable("fake_install_finalization_failure", 1, nullptr, 0);
    http_server_mock->clearReportEvents();

    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http_server_mock);
    EventHandler event_hdlr(aktualizr);
    aktualizr.Initialize();

    fiu_disable("fake_install_finalization_failure");

    EXPECT_FALSE(aktualizr.uptane_client()->hasPendingUpdates());
    EXPECT_TRUE(http_server_mock->checkReceivedReports({"EcuInstallationCompleted"}));
    EXPECT_FALSE(http_server_mock->wasInstallSuccessful(primary_ecu_id));

    data::InstallationResult dev_installation_res;
    std::string report;
    std::string correlation_id;

    // `device_installation_result` and `ecu_installation_results` are cleared
    // at finalizeAfterReboot()->putManifestSimple() once a device manifest is successfully sent to a server
    EXPECT_FALSE(storage->loadDeviceInstallationResult(&dev_installation_res, &report, &correlation_id));

    // it's used to return `true` even if there is no any record in DB
    // of the Uptane cycle just after sending manifest
    // made it consistent with loadDeviceInstallationResult
    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_installation_res;
    EXPECT_FALSE(storage->loadEcuInstallationResults(&ecu_installation_res));

    // verify currently installed version
    boost::optional<Uptane::Target> current_version;
    boost::optional<Uptane::Target> pending_version;

    ASSERT_TRUE(storage->loadInstalledVersions(primary_ecu_id, &current_version, &pending_version));
    EXPECT_FALSE(!!current_version);
    EXPECT_FALSE(!!pending_version);

    current_version = boost::none;
    pending_version = boost::none;

    ASSERT_TRUE(storage->loadInstalledVersions(secondary_ecu_id, &current_version, &pending_version));
    EXPECT_TRUE(!!current_version);
    EXPECT_FALSE(!!pending_version);
  }
}

/*
 * Initialize -> UptaneCycle -> download updates -> fail installation
 *
 * Verifies whether the Uptane client is not at pending state after installation failure
 *
 * Checks actions:
 *
 * - [x] Update is not in pending state anymore after failed installation
 * - [x] Store negative device installation result when an ECU installation failed
 */
TEST(Aktualizr, InstallationFailure) {
  std::vector<std::string> expected_event_order = {
      "UpdateCheckComplete",    "DownloadProgressReport", "DownloadTargetComplete", "DownloadProgressReport",
      "DownloadTargetComplete", "AllDownloadsComplete",   "InstallStarted",         "InstallTargetComplete",
      "InstallStarted",         "InstallTargetComplete",  "AllInstallsComplete",    "PutManifestComplete"};

  std::vector<std::string> expected_report_order = {
      "EcuDownloadStarted",     "EcuDownloadCompleted",     "EcuDownloadStarted",     "EcuDownloadCompleted",
      "EcuInstallationStarted", "EcuInstallationCompleted", "EcuInstallationStarted", "EcuInstallationCompleted"};

  const std::string primary_ecu_id = "CA:FE:A6:D2:84:9D";
  const std::string secondary_ecu_id = "secondary_ecu_serial";

  {
    TemporaryDirectory temp_dir;
    auto http_server_mock = std::make_shared<HttpInstallationFailed>(temp_dir.Path(), fake_meta_dir);
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http_server_mock->tls_server);
    auto storage = INvStorage::newStorage(conf.storage);

    fault_injection_init();
    fiu_enable("fake_package_install", 1, nullptr, 0);

    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http_server_mock);
    EventHandler event_hdlr(aktualizr);
    aktualizr.Initialize();

    // verify currently installed version
    boost::optional<Uptane::Target> current_version;
    boost::optional<Uptane::Target> pending_version;

    ASSERT_TRUE(storage->loadInstalledVersions(primary_ecu_id, &current_version, &pending_version));

    EXPECT_FALSE(!!pending_version);
    EXPECT_FALSE(!!current_version);

    aktualizr.UptaneCycle();
    aktualizr.uptane_client()->completeInstall();

    EXPECT_TRUE(event_hdlr.checkReceivedEvents(expected_event_order));
    EXPECT_FALSE(aktualizr.uptane_client()->hasPendingUpdates());
    EXPECT_TRUE(http_server_mock->checkReceivedReports(expected_report_order));
    EXPECT_FALSE(http_server_mock->wasInstallSuccessful(primary_ecu_id));
    EXPECT_TRUE(http_server_mock->wasInstallSuccessful(secondary_ecu_id));

    data::InstallationResult dev_installation_res;
    std::string report;
    std::string correlation_id;

    // `device_installation_result` and `ecu_installation_results` are cleared
    // at UptaneCycle()->putManifest() once a device manifest is successfully sent to a server
    EXPECT_FALSE(storage->loadDeviceInstallationResult(&dev_installation_res, &report, &correlation_id));

    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_installation_res;
    EXPECT_FALSE(storage->loadEcuInstallationResults(&ecu_installation_res));
    EXPECT_EQ(ecu_installation_res.size(), 0);

    ASSERT_TRUE(storage->loadInstalledVersions(primary_ecu_id, &current_version, &pending_version));
    // it says that no any installed version found,
    // which is, on one hand is correct since installation of the found update failed hence nothing was installed,
    // on the other hand some version should have been installed prior to the failed update
    EXPECT_FALSE(!!current_version);
    EXPECT_FALSE(!!pending_version);

    fiu_disable("fake_package_install");
  }

  // Primary and Secondary failure
  for (std::string prefix : {"secondary_install_", "secondary_sendfirmware_"}) {
    TemporaryDirectory temp_dir;
    auto http_server_mock = std::make_shared<HttpInstallationFailed>(temp_dir.Path(), fake_meta_dir);
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http_server_mock->tls_server);
    auto storage = INvStorage::newStorage(conf.storage);
    const std::string sec_fault_name = prefix + secondary_ecu_id;

    fault_injection_init();
    fault_injection_enable("fake_package_install", 1, "PRIMFAIL", 0);
    fault_injection_enable(sec_fault_name.c_str(), 1, "SECFAIL", 0);

    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http_server_mock);
    EventHandler event_hdlr(aktualizr);
    aktualizr.Initialize();

    // verify currently installed version
    boost::optional<Uptane::Target> current_version;
    boost::optional<Uptane::Target> pending_version;

    ASSERT_TRUE(storage->loadInstalledVersions(primary_ecu_id, &current_version, &pending_version));

    EXPECT_FALSE(!!pending_version);
    EXPECT_FALSE(!!current_version);

    aktualizr.UptaneCycle();
    aktualizr.uptane_client()->completeInstall();

    EXPECT_TRUE(event_hdlr.checkReceivedEvents(expected_event_order));
    EXPECT_FALSE(aktualizr.uptane_client()->hasPendingUpdates());
    EXPECT_TRUE(http_server_mock->checkReceivedReports(expected_report_order));
    EXPECT_FALSE(http_server_mock->wasInstallSuccessful(primary_ecu_id));
    EXPECT_FALSE(http_server_mock->wasInstallSuccessful(secondary_ecu_id));

    LOG_INFO << http_server_mock->last_manifest;
    Json::Value installation_report = http_server_mock->last_manifest["signed"]["installation_report"]["report"];
    EXPECT_EQ(installation_report["items"][0]["result"]["code"].asString(), "PRIMFAIL");
    EXPECT_EQ(installation_report["items"][1]["result"]["code"].asString(), "SECFAIL");
    EXPECT_EQ(installation_report["result"]["code"].asString(), "primary_hw:PRIMFAIL|secondary_hw:SECFAIL");

    data::InstallationResult dev_installation_res;
    std::string report;
    std::string correlation_id;

    // `device_installation_result` and `ecu_installation_results` are cleared
    // at UptaneCycle()->putManifest() once a device manifest is successfully sent to a server
    EXPECT_FALSE(storage->loadDeviceInstallationResult(&dev_installation_res, &report, &correlation_id));

    std::vector<std::pair<Uptane::EcuSerial, data::InstallationResult>> ecu_installation_res;
    EXPECT_FALSE(storage->loadEcuInstallationResults(&ecu_installation_res));
    EXPECT_EQ(ecu_installation_res.size(), 0);

    ASSERT_TRUE(storage->loadInstalledVersions(primary_ecu_id, &current_version, &pending_version));
    // it says that no any installed version found,
    // which is, on one hand is correct since installation of the found update failed hence nothing was installed,
    // on the other hand some version should have been installed prior to the failed update
    EXPECT_FALSE(!!current_version);
    EXPECT_FALSE(!!pending_version);

    fault_injection_disable("fake_package_install");
    fault_injection_disable(sec_fault_name.c_str());
  }
}

/*
 * Verifies that updates fail after metadata verification failure reported by Secondaries
 */
TEST(Aktualizr, SecondaryMetaFailure) {
  TemporaryDirectory temp_dir;
  auto http_server_mock = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http_server_mock->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);

  fault_injection_init();
  fiu_enable("secondary_putmetadata", 1, nullptr, 0);

  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http_server_mock);

  struct {
    result::Install result;
    std::promise<void> promise;
  } ev_state;

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    if (event->variant != "AllInstallsComplete") {
      return;
    }
    const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
    ev_state.result = installs_complete->result;

    ev_state.promise.set_value();
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);
  aktualizr.Initialize();
  aktualizr.UptaneCycle();

  auto status = ev_state.promise.get_future().wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }

  ASSERT_FALSE(ev_state.result.dev_report.isSuccess());

  fiu_disable("secondary_putmetadata");
}

#endif  // FIU_ENABLE

/*
 * Initialize -> UptaneCycle -> updates downloaded and installed for Primary
 * -> reboot emulated -> Initialize -> Finalize Installation After Reboot
 *
 * Verifies an auto reboot and pending updates finalization after a reboot
 * in case of the fake package manager usage
 *
 * Checks actions:
 *
 * - [x] Emulate a reboot at the end of the installation process in case of the fake package manager usage
 * - [x] Finalize a pending update that requires reboot
 */
TEST(Aktualizr, AutoRebootAfterUpdate) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakePutCounter>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  conf.pacman.fake_need_reboot = true;
  conf.uptane.force_install_completion = true;
  conf.uptane.polling_sec = 0;

  {
    // first run: do the install, exit UptaneCycle and emulate reboot since force_install_completion is set
    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();
    auto aktualizr_cycle_thread = aktualizr.RunForever();
    auto aktualizr_cycle_thread_status = aktualizr_cycle_thread.wait_for(std::chrono::seconds(20));
    aktualizr.Shutdown();

    EXPECT_EQ(aktualizr_cycle_thread_status, std::future_status::ready);
    EXPECT_TRUE(aktualizr.uptane_client()->isInstallCompletionRequired());
  }

  {
    // second run: after emulated reboot, check if the update was applied
    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();

    result::UpdateCheck update_res = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_res.status, result::UpdateStatus::kNoUpdatesAvailable);

    // Primary is installed, nothing pending
    boost::optional<Uptane::Target> current_target;
    boost::optional<Uptane::Target> pending_target;
    storage->loadPrimaryInstalledVersions(&current_target, &pending_target);
    EXPECT_TRUE(!!current_target);
    EXPECT_FALSE(!!pending_target);
    EXPECT_EQ(http->manifest_sends, 3);
  }
}

/*
 * Initialize -> UptaneCycle -> updates downloaded and installed for Secondaries
 * without changing the Primary.

 * Store installation result for Secondary
 */
TEST(Aktualizr, FullMultipleSecondaries) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "multisec", fake_meta_dir);
  Config conf("tests/config/basic.toml");
  conf.provision.primary_ecu_serial = "testecuserial";
  conf.provision.primary_ecu_hardware_id = "testecuhwid";
  conf.storage.path = temp_dir.Path();
  conf.pacman.images_path = temp_dir.Path() / "images";
  conf.tls.server = http->tls_server;
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";

  TemporaryDirectory temp_dir2;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "sec_serial1", "sec_hw1");

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir2, "sec_serial2", "sec_hw2");
  ASSERT_NO_THROW(aktualizr.AddSecondary(std::make_shared<Primary::VirtualSecondary>(
      Primary::VirtualSecondaryConfig::create_from_file(conf.uptane.secondary_config_file)[0])));

  struct {
    int started{0};
    int complete{0};
    bool allcomplete{false};
    bool manifest{false};
    std::future<void> future;
    std::promise<void> promise;
  } ev_state;
  ev_state.future = ev_state.promise.get_future();

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    if (event->variant == "InstallStarted") {
      ++ev_state.started;
    } else if (event->variant == "InstallTargetComplete") {
      ++ev_state.complete;
    } else if (event->variant == "AllInstallsComplete") {
      ev_state.allcomplete = true;
    } else if (event->variant == "PutManifestComplete") {
      ev_state.manifest = true;
    }
    // It is possible for the PutManifestComplete to come before we get the
    // InstallTargetComplete depending on the threading, so check for both.
    if (ev_state.allcomplete && ev_state.complete == 2 && ev_state.manifest) {
      ev_state.promise.set_value();
    }
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();

  auto status = ev_state.future.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }

  EXPECT_EQ(ev_state.started, 2);
  EXPECT_EQ(ev_state.complete, 2);

  const Json::Value manifest = http->last_manifest["signed"];
  const Json::Value manifest_versions = manifest["ecu_version_manifests"];

  // Make sure filepath were correctly written and formatted.
  EXPECT_EQ(manifest_versions["sec_serial1"]["signed"]["installed_image"]["filepath"].asString(),
            "secondary_firmware.txt");
  EXPECT_EQ(manifest_versions["sec_serial1"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 15);
  EXPECT_EQ(manifest_versions["sec_serial2"]["signed"]["installed_image"]["filepath"].asString(),
            "secondary_firmware2.txt");
  EXPECT_EQ(manifest_versions["sec_serial2"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 21);

  // Make sure there is no result for the Primary by checking the size
  EXPECT_EQ(manifest["installation_report"]["report"]["items"].size(), 2);
  EXPECT_EQ(manifest["installation_report"]["report"]["items"][0]["ecu"].asString(), "sec_serial1");
  EXPECT_TRUE(manifest["installation_report"]["report"]["items"][0]["result"]["success"].asBool());
  EXPECT_EQ(manifest["installation_report"]["report"]["items"][1]["ecu"].asString(), "sec_serial2");
  EXPECT_TRUE(manifest["installation_report"]["report"]["items"][1]["result"]["success"].asBool());
}

/*
 * Initialize -> CheckUpdates -> no updates -> no further action or events.
 */
TEST(Aktualizr, CheckNoUpdates) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  struct {
    size_t num_events{0};
    std::future<void> future;
    std::promise<void> promise;
  } ev_state;
  ev_state.future = ev_state.promise.get_future();

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    if (event->isTypeOf<event::DownloadProgressReport>()) {
      return;
    }
    LOG_INFO << "Got " << event->variant;
    switch (ev_state.num_events) {
      case 0: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 0);
        EXPECT_EQ(targets_event->result.updates.size(), 0);
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
        break;
      }
      case 1: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 0);
        EXPECT_EQ(targets_event->result.updates.size(), 0);
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
        ev_state.promise.set_value();
        break;
      }
      case 5:
        // Don't let the test run indefinitely!
        FAIL() << "Unexpected events!";
      default:
        std::cout << "event #" << ev_state.num_events << " is: " << event->variant << "\n";
        ASSERT_EQ(event->variant, "");
    }
    ++ev_state.num_events;
  };

  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  result::UpdateCheck result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(result.ecus_count, 0);
  EXPECT_EQ(result.updates.size(), 0);
  EXPECT_EQ(result.status, result::UpdateStatus::kNoUpdatesAvailable);
  // Fetch twice so that we can check for a second UpdateCheckComplete and
  // guarantee that nothing unexpected happened after the first fetch.
  result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(result.ecus_count, 0);
  EXPECT_EQ(result.updates.size(), 0);
  EXPECT_EQ(result.status, result::UpdateStatus::kNoUpdatesAvailable);

  auto status = ev_state.future.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(aktualizr.uptane_client()->AssembleManifest());
}

/*
 * Initialize -> Download -> nothing to download.
 *
 * Initialize -> CheckUpdates -> Download -> updates downloaded but not
 * installed.
 */
TEST(Aktualizr, DownloadWithUpdates) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  struct {
    size_t num_events{0};
    std::future<void> future;
    std::promise<void> promise;
  } ev_state;
  ev_state.future = ev_state.promise.get_future();

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    if (event->isTypeOf<event::DownloadProgressReport>()) {
      return;
    }
    LOG_INFO << "Got " << event->variant;
    switch (ev_state.num_events) {
      case 0: {
        ASSERT_EQ(event->variant, "AllDownloadsComplete");
        const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
        EXPECT_EQ(downloads_complete->result.updates.size(), 0);
        EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kError);
        break;
      }
      case 1: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 2);
        EXPECT_EQ(targets_event->result.updates.size(), 2u);
        EXPECT_EQ(targets_event->result.updates[0].filename(), "primary_firmware.txt");
        EXPECT_EQ(targets_event->result.updates[1].filename(), "secondary_firmware.txt");
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kUpdatesAvailable);
        break;
      }
      case 2:
      case 3: {
        ASSERT_EQ(event->variant, "DownloadTargetComplete");
        const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
        EXPECT_TRUE(download_event->update.filename() == "primary_firmware.txt" ||
                    download_event->update.filename() == "secondary_firmware.txt");
        EXPECT_TRUE(download_event->success);
        break;
      }
      case 4: {
        ASSERT_EQ(event->variant, "AllDownloadsComplete");
        const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
        EXPECT_EQ(downloads_complete->result.updates.size(), 2);
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "primary_firmware.txt" ||
                    downloads_complete->result.updates[1].filename() == "primary_firmware.txt");
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "secondary_firmware.txt" ||
                    downloads_complete->result.updates[1].filename() == "secondary_firmware.txt");
        EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kSuccess);
        ev_state.promise.set_value();
        break;
      }
      case 8:
        // Don't let the test run indefinitely!
        FAIL() << "Unexpected events!";
      default:
        std::cout << "event #" << ev_state.num_events << " is: " << event->variant << "\n";
        ASSERT_EQ(event->variant, "");
    }
    ++ev_state.num_events;
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  // First try downloading nothing. Nothing should happen.
  result::Download result = aktualizr.Download(std::vector<Uptane::Target>()).get();
  EXPECT_EQ(result.updates.size(), 0);
  EXPECT_EQ(result.status, result::DownloadStatus::kError);

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  aktualizr.Download(update_result.updates);

  auto status = ev_state.future.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for downloads to complete.";
  }

  verifyNothingInstalled(aktualizr.uptane_client()->AssembleManifest());
}

class HttpDownloadFailure : public HttpFake {
 public:
  using Responses = std::vector<std::pair<std::string, HttpResponse>>;

 public:
  HttpDownloadFailure(const boost::filesystem::path& test_dir_in, const Responses& file_to_response, std::string flavor,
                      const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, flavor, meta_dir_in) {
    for (auto resp : file_to_response) {
      url_to_response_[tls_server + target_dir_ + resp.first] = resp.second;
    }
  }

  HttpResponse download(const std::string& url, curl_write_callback write_cb, curl_xferinfo_callback progress_cb,
                        void* userp, curl_off_t from) override {
    const auto found_response_it = url_to_response_.find(url);
    if (found_response_it == url_to_response_.end()) {
      return HttpResponse("", 500, CURLE_HTTP_RETURNED_ERROR, "Internal Server Error");
    }

    auto response = url_to_response_.at(url);
    if (response.isOk()) {
      return HttpFake::download(url, write_cb, progress_cb, userp, from);
    }

    return url_to_response_[url];
  }

 private:
  const std::string target_dir_{"/repo/targets/"};
  std::map<std::string, HttpResponse> url_to_response_;
};

/**
 * Checks whether events are sent if download is unsuccessful
 *
 * Checks actions:
 * - Send DownloadTargetComplete event if download is unsuccessful
 * - Send AllDownloadsComplete event after partial download success
 * - Send AllDownloadsComplete event if all downloads are unsuccessful
 */
TEST(Aktualizr, DownloadFailures) {
  class DownloadEventHandler {
   public:
    DownloadEventHandler(Aktualizr& aktualizr) {
      functor_ = std::bind(&DownloadEventHandler::operator(), this, std::placeholders::_1);
      aktualizr.SetSignalHandler(functor_);
    }

    void operator()(const std::shared_ptr<event::BaseEvent>& event) {
      ASSERT_NE(event, nullptr);

      if (event->isTypeOf<event::DownloadTargetComplete>()) {
        auto download_target_complete_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
        auto target_filename = download_target_complete_event->update.filename();
        download_status[target_filename] = download_target_complete_event->success;

      } else if (event->isTypeOf<event::AllDownloadsComplete>()) {
        auto all_download_complete_event = dynamic_cast<event::AllDownloadsComplete*>(event.get());
        all_download_completed_status = all_download_complete_event->result;
      }
    }

   public:
    std::map<std::string, bool> download_status;
    result::Download all_download_completed_status;

   private:
    std::function<void(std::shared_ptr<event::BaseEvent>)> functor_;
  };

  struct TestParams {
    HttpDownloadFailure::Responses downloadResponse;
    std::vector<std::pair<std::string, bool>> downloadResult;
    result::DownloadStatus allDownloadsStatus;
  };

  TestParams test_case_params[]{
      {// test case 0: each target download fails
       {{"primary_firmware.txt", HttpResponse("", 500, CURLE_HTTP_RETURNED_ERROR, "Internal Server Error")},
        {"secondary_firmware.txt", HttpResponse("", 500, CURLE_HTTP_RETURNED_ERROR, "Internal Server Error")}},
       {{"primary_firmware.txt", false}, {"secondary_firmware.txt", false}},
       result::DownloadStatus::kError},
      {// test case 1: first target download succeeds, second target download fails
       {{"primary_firmware.txt", HttpResponse("", 200, CURLE_OK, "")},
        {"secondary_firmware.txt", HttpResponse("", 404, CURLE_HTTP_RETURNED_ERROR, "Not found")}},
       {{"primary_firmware.txt", true}, {"secondary_firmware.txt", false}},
       result::DownloadStatus::kPartialSuccess},
      {// test case 2: first target download fails, second target download succeeds
       {{"primary_firmware.txt", HttpResponse("", 404, CURLE_HTTP_RETURNED_ERROR, "Not found")},
        {"secondary_firmware.txt", HttpResponse("", 200, CURLE_OK, "")}},
       {{"primary_firmware.txt", false}, {"secondary_firmware.txt", true}},
       result::DownloadStatus::kPartialSuccess}};

  for (auto test_params : test_case_params) {
    TemporaryDirectory temp_dir;

    auto http = std::make_shared<HttpDownloadFailure>(temp_dir.Path(), test_params.downloadResponse, "hasupdates",
                                                      fake_meta_dir);
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
    auto storage = INvStorage::newStorage(conf.storage);

    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    DownloadEventHandler event_hdlr{aktualizr};

    aktualizr.Initialize();
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    ASSERT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
    result::Download download_result = aktualizr.Download(update_result.updates).get();

    for (auto& expected_result : test_params.downloadResult) {
      // check if DownloadTargetComplete was received
      ASSERT_NE(event_hdlr.download_status.find(expected_result.first), event_hdlr.download_status.end());
      EXPECT_EQ(event_hdlr.download_status.at(expected_result.first), expected_result.second);
    }
    EXPECT_EQ(event_hdlr.all_download_completed_status.status, test_params.allDownloadsStatus);
  }
}

/*
 * List targets in storage via API.
 * Remove targets in storage via API.
 */
TEST(Aktualizr, DownloadListRemove) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  aktualizr.Initialize();
  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();

  std::vector<Uptane::Target> targets = aktualizr.GetStoredTargets();
  EXPECT_EQ(targets.size(), 0);

  aktualizr.Download(update_result.updates).get();

  targets = aktualizr.GetStoredTargets();
  EXPECT_EQ(targets.size(), 2);

  // check that we can remove from the result of GetStoredTarget and
  // CheckUpdates
  aktualizr.DeleteStoredTarget(targets[0]);
  aktualizr.DeleteStoredTarget(update_result.updates[1]);

  targets = aktualizr.GetStoredTargets();
  EXPECT_EQ(targets.size(), 0);
}

/*
 * Automatically remove old targets during installation cycles.
 * Get log of installation.
 */
TEST(Aktualizr, TargetAutoremove) {
  TemporaryDirectory temp_dir;
  const boost::filesystem::path local_metadir = temp_dir / "metadir";
  Utils::createDirectories(local_metadir, S_IRWXU);
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "", local_metadir / "repo");

  UptaneRepo repo{local_metadir, "2025-07-04T16:33:27Z", "id0"};
  repo.generateRepo(KeyType::kED25519);
  const std::string hwid = "primary_hw";
  repo.addImage(fake_meta_dir / "fake_meta/primary_firmware.txt", "primary_firmware.txt", hwid, "", {});
  repo.addTarget("primary_firmware.txt", hwid, "CA:FE:A6:D2:84:9D", "");
  repo.signTargets();

  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  // attach the autoclean handler
  boost::signals2::connection ac_conn =
      aktualizr.SetSignalHandler(std::bind(targets_autoclean_cb, std::ref(aktualizr), std::placeholders::_1));
  aktualizr.Initialize();

  {
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    aktualizr.Download(update_result.updates).get();

    EXPECT_EQ(aktualizr.GetStoredTargets().size(), 1);

    result::Install install_result = aktualizr.Install(update_result.updates).get();
    EXPECT_TRUE(install_result.dev_report.success);

    std::vector<Uptane::Target> targets = aktualizr.GetStoredTargets();
    ASSERT_EQ(targets.size(), 1);
    EXPECT_EQ(targets[0].filename(), "primary_firmware.txt");
  }

  // second install
  repo.emptyTargets();
  repo.addImage(fake_meta_dir / "fake_meta/dummy_firmware.txt", "dummy_firmware.txt", hwid, "", {});
  repo.addTarget("dummy_firmware.txt", hwid, "CA:FE:A6:D2:84:9D", "");
  repo.signTargets();

  {
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    aktualizr.Download(update_result.updates).get();

    EXPECT_EQ(aktualizr.GetStoredTargets().size(), 2);

    result::Install install_result = aktualizr.Install(update_result.updates).get();
    EXPECT_TRUE(install_result.dev_report.success);

    // all targets are kept (current and previous)
    std::vector<Uptane::Target> targets = aktualizr.GetStoredTargets();
    ASSERT_EQ(targets.size(), 2);
  }

  // third install (first firmware again)
  repo.emptyTargets();
  repo.addImage(fake_meta_dir / "fake_meta/primary_firmware.txt", "primary_firmware.txt", hwid, "", {});
  repo.addTarget("primary_firmware.txt", hwid, "CA:FE:A6:D2:84:9D", "");
  repo.signTargets();

  {
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    aktualizr.Download(update_result.updates).get();

    EXPECT_EQ(aktualizr.GetStoredTargets().size(), 2);

    result::Install install_result = aktualizr.Install(update_result.updates).get();
    EXPECT_TRUE(install_result.dev_report.success);

    // all targets are kept again (current and previous)
    std::vector<Uptane::Target> targets = aktualizr.GetStoredTargets();
    ASSERT_EQ(targets.size(), 2);
  }

  // fourth install (some new third firmware)
  repo.emptyTargets();
  repo.addImage(fake_meta_dir / "fake_meta/secondary_firmware.txt", "secondary_firmware.txt", hwid, "", {});
  repo.addTarget("secondary_firmware.txt", hwid, "CA:FE:A6:D2:84:9D", "");
  repo.signTargets();

  {
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    aktualizr.Download(update_result.updates).get();

    EXPECT_EQ(aktualizr.GetStoredTargets().size(), 3);

    result::Install install_result = aktualizr.Install(update_result.updates).get();
    EXPECT_TRUE(install_result.dev_report.success);

    // only two targets are left: dummy_firmware has been cleaned up
    std::vector<Uptane::Target> targets = aktualizr.GetStoredTargets();
    ASSERT_EQ(targets.size(), 2);
  }

  Aktualizr::InstallationLog log = aktualizr.GetInstallationLog();
  ASSERT_EQ(log.size(), 2);

  EXPECT_EQ(log[0].installs.size(), 4);
  EXPECT_EQ(log[1].installs.size(), 0);

  std::vector<std::string> fws{"primary_firmware.txt", "dummy_firmware.txt", "primary_firmware.txt",
                               "secondary_firmware.txt"};
  for (auto it = log[0].installs.begin(); it < log[0].installs.end(); it++) {
    auto idx = static_cast<size_t>(it - log[0].installs.begin());
    EXPECT_EQ(it->filename(), fws[idx]);
  }
}

/*
 * Initialize -> Install -> nothing to install.
 *
 * Initialize -> CheckUpdates -> Download -> Install -> updates installed.
 */
TEST(Aktualizr, InstallWithUpdates) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  struct {
    size_t num_events{0};
    std::vector<Uptane::Target> updates;
    std::future<void> future;
    std::promise<void> promise;
  } ev_state;
  ev_state.future = ev_state.promise.get_future();

  auto f_cb = [&ev_state](const std::shared_ptr<event::BaseEvent>& event) {
    // Note that we do not expect a PutManifestComplete since we don't call
    // UptaneCycle() and that's the only function that generates that.
    if (event->isTypeOf<event::DownloadProgressReport>()) {
      return;
    }
    LOG_INFO << "Got " << event->variant;
    switch (ev_state.num_events) {
      case 0: {
        ASSERT_EQ(event->variant, "AllInstallsComplete");
        const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
        EXPECT_EQ(installs_complete->result.ecu_reports.size(), 0);
        break;
      }
      case 1: {
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
        EXPECT_EQ(targets_event->result.ecus_count, 2);
        EXPECT_EQ(targets_event->result.updates.size(), 2u);
        EXPECT_EQ(targets_event->result.updates[0].filename(), "primary_firmware.txt");
        EXPECT_EQ(targets_event->result.updates[1].filename(), "secondary_firmware.txt");
        EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kUpdatesAvailable);
        ev_state.updates = targets_event->result.updates;
        break;
      }
      case 2:
      case 3: {
        ASSERT_EQ(event->variant, "DownloadTargetComplete");
        const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
        EXPECT_TRUE(download_event->update.filename() == "primary_firmware.txt" ||
                    download_event->update.filename() == "secondary_firmware.txt");
        EXPECT_TRUE(download_event->success);
        break;
      }
      case 4: {
        ASSERT_EQ(event->variant, "AllDownloadsComplete");
        const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
        EXPECT_EQ(downloads_complete->result.updates.size(), 2);
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "primary_firmware.txt" ||
                    downloads_complete->result.updates[1].filename() == "primary_firmware.txt");
        EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "secondary_firmware.txt" ||
                    downloads_complete->result.updates[1].filename() == "secondary_firmware.txt");
        EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kSuccess);
        break;
      }
      case 5: {
        // Primary always gets installed first. (Not a requirement, just how it
        // works at present.)
        ASSERT_EQ(event->variant, "InstallStarted");
        const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
        EXPECT_EQ(install_started->serial.ToString(), "CA:FE:A6:D2:84:9D");
        break;
      }
      case 6: {
        // Primary should complete before Secondary begins. (Again not a
        // requirement per se.)
        ASSERT_EQ(event->variant, "InstallTargetComplete");
        const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
        EXPECT_EQ(install_complete->serial.ToString(), "CA:FE:A6:D2:84:9D");
        EXPECT_TRUE(install_complete->success);
        break;
      }
      case 7: {
        ASSERT_EQ(event->variant, "InstallStarted");
        const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
        EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
        break;
      }
      case 8: {
        ASSERT_EQ(event->variant, "InstallTargetComplete");
        const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
        EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
        EXPECT_TRUE(install_complete->success);
        break;
      }
      case 9: {
        ASSERT_EQ(event->variant, "AllInstallsComplete");
        const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
        EXPECT_EQ(installs_complete->result.ecu_reports.size(), 2);
        EXPECT_EQ(installs_complete->result.ecu_reports[0].install_res.result_code.num_code,
                  data::ResultCode::Numeric::kOk);
        EXPECT_EQ(installs_complete->result.ecu_reports[1].install_res.result_code.num_code,
                  data::ResultCode::Numeric::kOk);
        ev_state.promise.set_value();
        break;
      }
      case 12:
        // Don't let the test run indefinitely!
        FAIL() << "Unexpected events!";
      default:
        std::cout << "event #" << ev_state.num_events << " is: " << event->variant << "\n";
        ASSERT_EQ(event->variant, "");
    }
    ++ev_state.num_events;
  };

  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  Json::Value primary_json;
  primary_json["hashes"]["sha256"] = "74e653bbf6c00a88b21f0992159b1002f5af38506e6d2fbc7eb9846711b2d75f";
  primary_json["hashes"]["sha512"] =
      "91814ad1c13ebe2af8d65044893633c4c3ce964edb8cb58b0f357406c255f7be94f42547e108b300346a42cd57662e4757b9d843b7acbc09"
      "0df0bc05fe55297f";
  primary_json["length"] = 59;
  Uptane::Target primary_target("primary_firmware.txt", primary_json);

  Json::Value secondary_json;
  secondary_json["hashes"]["sha256"] = "1bbb15aa921ffffd5079567d630f43298dbe5e7cbc1b14e0ccdd6718fde28e47";
  secondary_json["hashes"]["sha512"] =
      "7dbae4c36a2494b731a9239911d3085d53d3e400886edb4ae2b9b78f40bda446649e83ba2d81653f614cc66f5dd5d4dbd95afba854f148af"
      "bfae48d0ff4cc38a";
  secondary_json["length"] = 15;
  Uptane::Target secondary_target("secondary_firmware.txt", secondary_json);

  // First try installing nothing. Nothing should happen.
  result::Install result = aktualizr.Install(ev_state.updates).get();
  EXPECT_EQ(result.ecu_reports.size(), 0);

  EXPECT_THROW(aktualizr.OpenStoredTarget(primary_target).get(), std::runtime_error)
      << "Primary firmware is present in storage before the download";
  EXPECT_THROW(aktualizr.OpenStoredTarget(secondary_target).get(), std::runtime_error)
      << "Secondary firmware is present in storage before the download";

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  aktualizr.Download(update_result.updates).get();
  EXPECT_NO_THROW(aktualizr.OpenStoredTarget(primary_target))
      << "Primary firmware is not present in storage after the download";
  EXPECT_NO_THROW(aktualizr.OpenStoredTarget(secondary_target))
      << "Secondary firmware is not present in storage after the download";

  // After updates have been downloaded, try to install them.
  aktualizr.Install(update_result.updates);

  auto status = ev_state.future.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }
}

/**
 * Verifies reporting of update download progress
 *
 * Checks actions:
 *
 * - [x] Report download progress
 */
TEST(Aktualizr, ReportDownloadProgress) {
  // The test initialization part is repeated in many tests so maybe it makes sense
  // to define a fixture and move this common initialization part into the fixture's SetUp() method
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates", fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  unsigned int report_counter = {0};
  std::shared_ptr<const event::DownloadProgressReport> lastProgressReport{nullptr};

  std::function<void(std::shared_ptr<event::BaseEvent> event)> report_event_hdlr =
      [&](const std::shared_ptr<event::BaseEvent>& event) {
        ASSERT_NE(event, nullptr);
        if (!event->isTypeOf<event::DownloadProgressReport>()) {
          return;
        }

        auto download_progress_event = std::dynamic_pointer_cast<event::DownloadProgressReport>(event);
        ASSERT_NE(download_progress_event, nullptr);
        ASSERT_NE(download_progress_event.get(), nullptr);
        if (lastProgressReport) {
          EXPECT_GE(download_progress_event->progress, lastProgressReport->progress);
        }
        lastProgressReport = download_progress_event;
        ++report_counter;
      };

  aktualizr.SetSignalHandler(report_event_hdlr);
  aktualizr.Initialize();

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  ASSERT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  // The test mocks are tailored to emulate a device with a Primary ECU and one Secondary ECU
  // for sake of the download progress report testing it's suffice to test it agains just one of the ECUs
  update_result.updates.pop_back();

  result::Download download_result = aktualizr.Download(update_result.updates).get();
  EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

  ASSERT_NE(lastProgressReport, nullptr);
  ASSERT_NE(lastProgressReport.get(), nullptr);
  EXPECT_TRUE(event::DownloadProgressReport::isDownloadCompleted(*lastProgressReport));
  EXPECT_GT(report_counter, 1);
}

class HttpFakeCampaign : public HttpFake {
 public:
  HttpFakeCampaign(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "", meta_dir_in) {}

  HttpResponse get(const std::string& url, int64_t maxsize) override {
    EXPECT_NE(url.find("campaigner/"), std::string::npos);
    boost::filesystem::path path = meta_dir / url.substr(tls_server.size() + strlen("campaigner/"));

    if (url.find("campaigner/campaigns") != std::string::npos) {
      return HttpResponse(Utils::readFile(path.parent_path() / "campaigner/campaigns.json"), 200, CURLE_OK, "");
    }
    return HttpFake::get(url, maxsize);
  }

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    for (auto it = data.begin(); it != data.end(); it++) {
      auto ev = *it;
      auto id = ev["eventType"]["id"];
      if (id == "campaign_accepted" || id == "campaign_declined" || id == "campaign_postponed") {
        seen_events.push_back(ev);
      }
    }
    return HttpResponse("", 204, CURLE_OK, "");
  }

  std::vector<Json::Value> seen_events;
};

class CampaignEvents {
 public:
  bool campaignaccept_seen{false};
  bool campaigndecline_seen{false};
  bool campaignpostpone_seen{false};

  void handler(const std::shared_ptr<event::BaseEvent>& event) {
    std::cout << event->variant << "\n";
    if (event->variant == "CampaignCheckComplete") {
      auto concrete_event = std::static_pointer_cast<event::CampaignCheckComplete>(event);
      EXPECT_EQ(concrete_event->result.campaigns.size(), 1);
      EXPECT_EQ(concrete_event->result.campaigns[0].name, "campaign1");
      EXPECT_EQ(concrete_event->result.campaigns[0].id, "c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493");
      EXPECT_EQ(concrete_event->result.campaigns[0].size, 62470);
      EXPECT_EQ(concrete_event->result.campaigns[0].autoAccept, true);
      EXPECT_EQ(concrete_event->result.campaigns[0].description, "this is my message to show on the device");
    } else if (event->variant == "CampaignAcceptComplete") {
      campaignaccept_seen = true;
    } else if (event->variant == "CampaignDeclineComplete") {
      campaigndecline_seen = true;
    } else if (event->variant == "CampaignPostponeComplete") {
      campaignpostpone_seen = true;
    }
  }
};

/* Check for campaigns with manual control.
 * Send CampaignCheckComplete event with campaign data.
 * Fetch campaigns from the server.
 *
 * Accept a campaign.
 * Send campaign acceptance report.
 * Send CampaignAcceptComplete event.
 *
 * Decline a campaign.
 * Send campaign decline report.
 * Send CampaignDeclineComplete event.
 *
 * Postpone a campaign.
 * Send campaign postpone report.
 * Send CampaignPostponeComplete event.
 */
TEST(Aktualizr, CampaignCheckAndControl) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeCampaign>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  CampaignEvents campaign_events;

  {
    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    aktualizr.SetSignalHandler(std::bind(&CampaignEvents::handler, &campaign_events, std::placeholders::_1));
    aktualizr.Initialize();

    // check for campaign
    auto result = aktualizr.CampaignCheck().get();
    EXPECT_EQ(result.campaigns.size(), 1);

    // accept the campaign
    aktualizr.CampaignControl("c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493", campaign::Cmd::Accept).get();

    aktualizr.CampaignControl("c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493", campaign::Cmd::Decline).get();

    aktualizr.CampaignControl("c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493", campaign::Cmd::Postpone).get();
  }

  ASSERT_EQ(http->seen_events.size(), 3);
  for (const auto& ev : http->seen_events) {
    EXPECT_EQ(ev["event"]["campaignId"], "c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493");
  }
  EXPECT_TRUE(campaign_events.campaignaccept_seen);
  EXPECT_TRUE(campaign_events.campaigndecline_seen);
  EXPECT_TRUE(campaign_events.campaignpostpone_seen);
}

class HttpFakeNoCorrelationId : public HttpFake {
 public:
  HttpFakeNoCorrelationId(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "", meta_dir_in) {}

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    for (const Json::Value& event : data) {
      ++events_seen;
      EXPECT_EQ(event["event"]["correlationId"].asString(), "");
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  unsigned int events_seen{0};
};

/* Correlation ID is empty if none was provided in Targets metadata. */
TEST(Aktualizr, FullNoCorrelationId) {
  TemporaryDirectory temp_dir;
  TemporaryDirectory meta_dir;
  Utils::copyDir(uptane_repos_dir / "full_no_correlation_id/repo/repo", meta_dir.Path() / "repo");
  Utils::copyDir(uptane_repos_dir / "full_no_correlation_id/repo/director", meta_dir.Path() / "director");
  auto http = std::make_shared<HttpFakeNoCorrelationId>(temp_dir.Path(), meta_dir.Path());

  // scope `Aktualizr` object, so that the ReportQueue flushes its events before
  // we count them at the end
  {
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    result::Install install_result = aktualizr.Install(download_result.updates).get();
    for (const auto& r : install_result.ecu_reports) {
      EXPECT_EQ(r.install_res.result_code.num_code, data::ResultCode::Numeric::kOk);
    }
  }

  EXPECT_EQ(http->events_seen, 8);
}

TEST(Aktualizr, ManifestCustom) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "", fake_meta_dir);

  {
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

    auto storage = INvStorage::newStorage(conf.storage);
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();
    Json::Value custom = Utils::parseJSON(R"({"test_field":"test_value"})");
    ASSERT_EQ(custom["test_field"].asString(), "test_value");  // Shouldn't fail, just check that test itself is correct
    ASSERT_EQ(true, aktualizr.SendManifest(custom).get()) << "Failed to upload manifest with HttpFake server";
    EXPECT_EQ(http->last_manifest["signed"]["custom"], custom);
  }
}

TEST(Aktualizr, CustomInstallationRawReport) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdate", fake_meta_dir);

  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);

  aktualizr.Initialize();
  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  result::Download download_result = aktualizr.Download(update_result.updates).get();
  result::Install install_result = aktualizr.Install(download_result.updates).get();

  auto custom_raw_report = "Installation's custom raw report!";
  EXPECT_TRUE(aktualizr.SetInstallationRawReport(custom_raw_report));
  aktualizr.SendManifest().get();
  EXPECT_EQ(http->last_manifest["signed"]["installation_report"]["report"]["raw_report"], custom_raw_report);

  // After sending manifest, an installation report will be removed from the DB,
  // so Aktualzr::SetInstallationRawReport must return a negative value.
  EXPECT_FALSE(aktualizr.SetInstallationRawReport(custom_raw_report));
}

class CountUpdateCheckEvents {
 public:
  CountUpdateCheckEvents() = default;
  // Non-copyable
  CountUpdateCheckEvents(const CountUpdateCheckEvents&) = delete;
  CountUpdateCheckEvents& operator=(const CountUpdateCheckEvents&) = delete;

  std::function<void(std::shared_ptr<event::BaseEvent>)> Signal() {
    return std::bind(&CountUpdateCheckEvents::count, this, std::placeholders::_1);
  }

  void count(std::shared_ptr<event::BaseEvent> event) {
    std::cout << event->variant << "\n";
    if (event->variant == "UpdateCheckComplete") {
      total_events_++;
      if (std::static_pointer_cast<event::UpdateCheckComplete>(event)->result.status == result::UpdateStatus::kError) {
        error_events_++;
      }
    }
  }

  int total_events() const { return total_events_; }
  int error_events() const { return error_events_; }

 private:
  int total_events_{0};
  int error_events_{0};
};

TEST(Aktualizr, APICheck) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates", fake_meta_dir);

  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);

  CountUpdateCheckEvents counter;
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    boost::signals2::connection conn = aktualizr.SetSignalHandler(counter.Signal());
    aktualizr.Initialize();
    std::vector<std::future<result::UpdateCheck>> futures;
    for (int i = 0; i < 5; ++i) {
      futures.push_back(aktualizr.CheckUpdates());
    }

    for (auto& f : futures) {
      f.get();
    }
  }

  EXPECT_EQ(counter.total_events(), 5);

  // try again, but shutdown before it finished all calls
  CountUpdateCheckEvents counter2;
  {
    UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
    boost::signals2::connection conn = aktualizr.SetSignalHandler(counter2.Signal());
    aktualizr.Initialize();

    std::future<result::UpdateCheck> ft = aktualizr.CheckUpdates();
    for (int i = 0; i < 99; ++i) {
      aktualizr.CheckUpdates();
    }
    ft.get();
  }
  EXPECT_LT(counter2.total_events(), 100);
  EXPECT_GT(counter2.total_events(), 0);
}

class HttpPutManifestFail : public HttpFake {
 public:
  HttpPutManifestFail(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "", meta_dir_in) {}
  HttpResponse put(const std::string& url, const Json::Value& data) override {
    (void)data;
    return HttpResponse(url, 504, CURLE_OK, "");
  }
};

/* Send UpdateCheckComplete event after failure */
TEST(Aktualizr, UpdateCheckCompleteError) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpPutManifestFail>(temp_dir.Path(), fake_meta_dir);

  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, "http://updatefail");

  auto storage = INvStorage::newStorage(conf.storage);
  CountUpdateCheckEvents counter;

  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  boost::signals2::connection conn = aktualizr.SetSignalHandler(counter.Signal());
  aktualizr.Initialize();
  auto result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(result.status, result::UpdateStatus::kError);
  EXPECT_EQ(counter.error_events(), 1);
}

/*
 * Suspend API calls during pause.
 * Catch up with calls queue after resume.
 */

class HttpFakePauseCounter : public HttpFake {
 public:
  HttpFakePauseCounter(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "noupdates", meta_dir_in) {}

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    for (const Json::Value& event : data) {
      ++events_seen;
      std::string event_type = event["eventType"]["id"].asString();

      std::cout << "got event #" << events_seen << ": " << event_type << "\n";
      if (events_seen == 1) {
        EXPECT_EQ(event_type, "DevicePaused");
        EXPECT_EQ(event["event"]["correlationId"], "id0");
      } else if (events_seen == 2) {
        EXPECT_EQ(event_type, "DeviceResumed");
        EXPECT_EQ(event["event"]["correlationId"], "id0");
      } else {
        std::cout << "Unexpected event";
        EXPECT_EQ(0, 1);
      }
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  unsigned int events_seen{0};
};

TEST(Aktualizr, PauseResumeQueue) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakePauseCounter>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();

  std::mutex mutex;
  std::promise<void> end_promise{};
  size_t n_events = 0;
  std::atomic_bool is_paused{false};
  std::function<void(std::shared_ptr<event::BaseEvent>)> cb = [&end_promise, &n_events, &mutex,
                                                               &is_paused](std::shared_ptr<event::BaseEvent> event) {
    switch (n_events) {
      case 0:
        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        break;
      case 1: {
        std::lock_guard<std::mutex> guard(mutex);

        ASSERT_EQ(event->variant, "UpdateCheckComplete");
        // the event shouldn't happen when the system is paused
        EXPECT_FALSE(is_paused);
        end_promise.set_value();
        break;
      }
      default:
        FAIL() << "Unexpected event";
    }
    n_events += 1;
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(cb);

  // trigger the first UpdateCheck
  aktualizr.CheckUpdates().get();

  {
    std::lock_guard<std::mutex> guard(mutex);
    EXPECT_EQ(aktualizr.Pause().status, result::PauseStatus::kSuccess);
    is_paused = true;
  }

  EXPECT_EQ(aktualizr.Pause().status, result::PauseStatus::kAlreadyPaused);

  aktualizr.CheckUpdates();

  // Theoritically racy, could cause bad implem to succeeed sometimes
  std::this_thread::sleep_for(std::chrono::seconds(1));

  {
    std::lock_guard<std::mutex> guard(mutex);
    is_paused = false;
    EXPECT_EQ(aktualizr.Resume().status, result::PauseStatus::kSuccess);
  }

  EXPECT_EQ(aktualizr.Resume().status, result::PauseStatus::kAlreadyRunning);

  auto status = end_promise.get_future().wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for UpdateCheck event";
  }
}

const std::string custom_hwinfo =
    R"([{"description":"ECU1","ECU P/N":"AAA","HW P/N":"BBB","HW Version":"1.234",
  "SW P/N":"CCC","SW Version":"4.321","ECU Serial":"AAA-BBB-CCC"},
  {"description":"ECU2","ECU P/N":"ZZZ","HW P/N":"XXX","HW Version":"9.876",
  "SW P/N":"YYY","SW Version":"6.789","ECU Serial":"VVV-NNN-MMM"}])";

class HttpSystemInfo : public HttpFake {
 public:
  HttpSystemInfo(const boost::filesystem::path& test_dir_in, const boost::filesystem::path& meta_dir_in)
      : HttpFake(test_dir_in, "", meta_dir_in) {}

  HttpResponse put(const std::string& url, const Json::Value& data) override {
    if (url.find(hwinfo_ep_) == url.length() - hwinfo_ep_.length()) {
      if (info_count_ == 0) {  // expect lshw data
        EXPECT_TRUE(data.isObject());
        EXPECT_TRUE(data.isMember("description"));
      } else if (info_count_ == 1) {  // expect custom data
        auto hwinfo = Utils::parseJSON(custom_hwinfo);
        EXPECT_EQ(hwinfo, data);
      }
      info_count_++;
      return HttpResponse("", 200, CURLE_OK, "");
    } else if (url.find("/manifest") != std::string::npos) {
      return HttpResponse("", 200, CURLE_OK, "");
    }
    return HttpResponse("", 404, CURLE_HTTP_RETURNED_ERROR, "Not found");
  }

  ~HttpSystemInfo() { EXPECT_EQ(info_count_, 2); }

  int info_count_{0};
  std::string hwinfo_ep_{"/system_info"};
};

TEST(Aktualizr, CustomHwInfo) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpSystemInfo>(temp_dir.Path(), fake_meta_dir);
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  UptaneTestCommon::TestAktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();

  aktualizr.SendDeviceData().get();

  auto hwinfo = Utils::parseJSON(custom_hwinfo);
  aktualizr.SendDeviceData(hwinfo).get();
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the base directory of Uptane repos.\n";
    return EXIT_FAILURE;
  }
  uptane_repos_dir = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  TemporaryDirectory tmp_dir;
  fake_meta_dir = tmp_dir.Path();
  MetaFake meta_fake(fake_meta_dir);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
