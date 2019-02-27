#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "config/config.h"
#include "httpfake.h"
#include "primary/aktualizr.h"
#include "primary/events.h"
#include "primary/sotauptaneclient.h"
#include "uptane/secondaryfactory.h"
#include "uptane_test_common.h"
#include "utilities/utils.h"

boost::filesystem::path uptane_repos_dir;

void verifyNothingInstalled(const Json::Value& manifest) {
  // Verify nothing has installed for the primary.
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
  // Verify nothing has installed for the secondary.
  EXPECT_EQ(
      manifest["ecu_version_manifests"]["secondary_ecu_serial"]["signed"]["installed_image"]["filepath"].asString(),
      "noimage");
}

int num_events_FullNoUpdates = 0;
std::future<void> future_FullNoUpdates{};
std::promise<void> promise_FullNoUpdates{};
void process_events_FullNoUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->isTypeOf(event::DownloadProgressReport::TypeName)) {
    return;
  }
  LOG_INFO << "Got " << event->variant;
  switch (num_events_FullNoUpdates) {
    case 0: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
      const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
      EXPECT_EQ(targets_event->result.ecus_count, 0);
      EXPECT_EQ(targets_event->result.updates.size(), 0);
      EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
      break;
    }
    case 1: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
      const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
      EXPECT_EQ(targets_event->result.ecus_count, 0);
      EXPECT_EQ(targets_event->result.updates.size(), 0);
      EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
      promise_FullNoUpdates.set_value();
      break;
    }
    case 5:
      // Don't let the test run indefinitely!
      FAIL() << "Unexpected events!";
    default:
      std::cout << "event #" << num_events_FullNoUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_FullNoUpdates;
}

/*
 * Initialize -> UptaneCycle -> no updates -> no further action or events.
 */
TEST(Aktualizr, FullNoUpdates) {
  future_FullNoUpdates = promise_FullNoUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FullNoUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();
  // Run the cycle twice so that we can check for a second UpdateCheckComplete
  // and guarantee that nothing unexpected happened after the first fetch.
  aktualizr.UptaneCycle();
  std::future_status status = future_FullNoUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(aktualizr.uptane_client_->AssembleManifest());
}

/*
 * Add secondaries via API
 */
TEST(Aktualizr, AddSecondary) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::SecondaryType::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path();
  ecu_config.ecu_serial = "ecuserial3";
  ecu_config.ecu_hardware_id = "hw_id3";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir / "firmware.txt";
  ecu_config.target_name_path = temp_dir / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir / "secondary_metadata";

  aktualizr.AddSecondary(Uptane::SecondaryFactory::makeSecondary(ecu_config));

  aktualizr.Initialize();

  EcuSerials serials;
  storage->loadEcuSerials(&serials);

  std::vector<std::string> expected_ecus = {"CA:FE:A6:D2:84:9D", "ecuserial3", "secondary_ecu_serial"};
  EXPECT_EQ(serials.size(), 3);
  for (const auto& ecu : serials) {
    auto found = std::find(expected_ecus.begin(), expected_ecus.end(), ecu.first.ToString());
    if (found != expected_ecus.end()) {
      expected_ecus.erase(found);
    } else {
      FAIL() << "Unknown ecu: " << ecu.first.ToString();
    }
  }
  EXPECT_EQ(expected_ecus.size(), 0);

  ecu_config.ecu_serial = "ecuserial4";
  auto sec4 = Uptane::SecondaryFactory::makeSecondary(ecu_config);
  EXPECT_THROW(aktualizr.AddSecondary(sec4), std::logic_error);
}

class HttpFakeEventCounter : public HttpFake {
 public:
  HttpFakeEventCounter(const boost::filesystem::path& test_dir_in) : HttpFake(test_dir_in, "hasupdates") {}

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

int num_events_FullWithUpdates = 0;
std::future<void> future_FullWithUpdates{};
std::promise<void> promise_FullWithUpdates{};
void process_events_FullWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->isTypeOf(event::DownloadProgressReport::TypeName)) {
    return;
  }
  LOG_INFO << "Got " << event->variant;
  switch (num_events_FullWithUpdates) {
    case 0: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
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
      EXPECT_EQ(event->variant, "DownloadTargetComplete");
      const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
      EXPECT_TRUE(download_event->update.filename() == "primary_firmware.txt" ||
                  download_event->update.filename() == "secondary_firmware.txt");
      EXPECT_TRUE(download_event->success);
      break;
    }
    case 3: {
      EXPECT_EQ(event->variant, "AllDownloadsComplete");
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
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "CA:FE:A6:D2:84:9D");
      break;
    }
    case 5: {
      // Primary should complete before secondary begins. (Again not a
      // requirement per se.)
      EXPECT_EQ(event->variant, "InstallTargetComplete");
      const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "CA:FE:A6:D2:84:9D");
      EXPECT_TRUE(install_complete->success);
      break;
    }
    case 6: {
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
      break;
    }
    case 7: {
      EXPECT_EQ(event->variant, "InstallTargetComplete");
      const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
      EXPECT_TRUE(install_complete->success);
      break;
    }
    case 8: {
      EXPECT_EQ(event->variant, "AllInstallsComplete");
      const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
      EXPECT_EQ(installs_complete->result.ecu_reports.size(), 2);
      EXPECT_EQ(installs_complete->result.ecu_reports[0].install_res.result_code.num_code,
                data::ResultCode::Numeric::kOk);
      EXPECT_EQ(installs_complete->result.ecu_reports[1].install_res.result_code.num_code,
                data::ResultCode::Numeric::kOk);
      break;
    }
    case 9: {
      EXPECT_EQ(event->variant, "PutManifestComplete");
      const auto put_complete = dynamic_cast<event::PutManifestComplete*>(event.get());
      EXPECT_TRUE(put_complete->success);
      promise_FullWithUpdates.set_value();
      break;
    }
    case 12:
      // Don't let the test run indefinitely!
      FAIL() << "Unexpected events!";
    default:
      std::cout << "event #" << num_events_FullWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_FullWithUpdates;
}

/*
 * Initialize -> UptaneCycle -> updates downloaded and installed for primary and
 * secondary.
 */
TEST(Aktualizr, FullWithUpdates) {
  future_FullWithUpdates = promise_FullWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeEventCounter>(temp_dir.Path());
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FullWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();
  std::future_status status = future_FullWithUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }
  EXPECT_EQ(http->events_seen, 8);
}

class HttpFakePutCounter : public HttpFake {
 public:
  HttpFakePutCounter(const boost::filesystem::path& test_dir_in) : HttpFake(test_dir_in, "hasupdates") {}

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
 * Initialize -> UptaneCycle -> updates downloaded and installed for primary
 * (after reboot) and secondary (aktualizr_test.cc)
 *
 * It simulates closely the OStree case which needs a reboot after applying an
 * update, but uses `PackageManagerFake`.
 *
 * Checks actions:
 *
 * - [x] Finalize a pending update that requires reboot
 *   - [x] Store installation result
 *   - [x] Send manifest
 *   - [x] Update is not in pending state anymore after successful finalization
 *
 *  - [x] Install an update on the primary
 *    - [x] Set new version to pending status after an OSTree update trigger
 *    - [x] Send EcuInstallationAppliedReport to server after an OSTree update trigger
 *    - [x] Uptane check for updates and manifest sends are disabled while an installation
 *          is pending reboot
 */
TEST(Aktualizr, FullWithUpdatesNeedReboot) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakePutCounter>(temp_dir.Path());
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  conf.pacman.fake_need_reboot = true;
  conf.bootloader.reboot_sentinel_dir = temp_dir.Path();

  {
    // first run: do the install
    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();
    aktualizr.UptaneCycle();

    // check that a version is here, set to pending

    size_t pending_target = SIZE_MAX;
    std::vector<Uptane::Target> targets;
    storage->loadPrimaryInstalledVersions(&targets, nullptr, &pending_target);
    EXPECT_NE(pending_target, SIZE_MAX);
  }

  // check that no manifest has been sent after the update application
  EXPECT_EQ(http->manifest_sends, 1);
  EXPECT_EQ(http->count_event_with_type("EcuInstallationStarted"), 2);    // two ecus have started
  EXPECT_EQ(http->count_event_with_type("EcuInstallationApplied"), 1);    // primary ecu has been applied
  EXPECT_EQ(http->count_event_with_type("EcuInstallationCompleted"), 1);  // secondary ecu has been updated

  {
    // second run: before reboot, re-use the storage
    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();

    // check that everything is still pending
    size_t pending_target = SIZE_MAX;
    std::vector<Uptane::Target> targets;
    storage->loadPrimaryInstalledVersions(&targets, nullptr, &pending_target);
    EXPECT_LT(pending_target, targets.size());

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
    Aktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();

    result::UpdateCheck update_res = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_res.status, result::UpdateStatus::kNoUpdatesAvailable);

    // primary is installed, nothing pending
    size_t current_target = SIZE_MAX;
    size_t pending_target = SIZE_MAX;
    std::vector<Uptane::Target> targets;
    storage->loadPrimaryInstalledVersions(&targets, &current_target, &pending_target);
    EXPECT_LT(current_target, targets.size());
    EXPECT_EQ(pending_target, SIZE_MAX);

    // secondary is installed, nothing pending
    size_t sec_current_target = SIZE_MAX;
    size_t sec_pending_target = SIZE_MAX;
    std::vector<Uptane::Target> sec_targets;
    storage->loadInstalledVersions("secondary_ecu_serial", &sec_targets, &sec_current_target, &sec_pending_target);
    EXPECT_LT(sec_current_target, sec_targets.size());
    EXPECT_EQ(sec_pending_target, SIZE_MAX);
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

/*
 * Initialize -> UptaneCycle -> updates downloaded and installed for primary
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
  auto http = std::make_shared<HttpFakePutCounter>(temp_dir.Path());
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  conf.pacman.fake_need_reboot = true;
  conf.uptane.force_install_completion = true;
  conf.uptane.polling_sec = 0;
  conf.bootloader.reboot_sentinel_dir = temp_dir.Path();

  {
    // first run: do the install, exit UptaneCycle and emulate reboot since force_install_completion is set
    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();
    auto aktualizr_cycle_thread = aktualizr.RunForever();
    auto aktualizr_cycle_thread_status = aktualizr_cycle_thread.wait_for(std::chrono::seconds(10));

    EXPECT_EQ(aktualizr_cycle_thread_status, std::future_status::ready);
    EXPECT_TRUE(aktualizr.uptane_client_->bootloader->rebootDetected());
  }

  {
    // second run: after emulated reboot, check if the update was applied
    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();

    result::UpdateCheck update_res = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_res.status, result::UpdateStatus::kNoUpdatesAvailable);

    // primary is installed, nothing pending
    size_t current_target = SIZE_MAX;
    size_t pending_target = SIZE_MAX;
    std::vector<Uptane::Target> targets;
    storage->loadPrimaryInstalledVersions(&targets, &current_target, &pending_target);
    EXPECT_LT(current_target, targets.size());
    EXPECT_EQ(pending_target, SIZE_MAX);
    EXPECT_EQ(http->manifest_sends, 4);
  }
}

int started_FullMultipleSecondaries = 0;
int complete_FullMultipleSecondaries = 0;
bool allcomplete_FullMultipleSecondaries = false;
bool manifest_FullMultipleSecondaries = false;
std::future<void> future_FullMultipleSecondaries{};
std::promise<void> promise_FullMultipleSecondaries{};
void process_events_FullMultipleSecondaries(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "InstallStarted") {
    ++started_FullMultipleSecondaries;
  } else if (event->variant == "InstallTargetComplete") {
    ++complete_FullMultipleSecondaries;
  } else if (event->variant == "AllInstallsComplete") {
    allcomplete_FullMultipleSecondaries = true;
  } else if (event->variant == "PutManifestComplete") {
    manifest_FullMultipleSecondaries = true;
  }
  // It is possible for the PutManifestComplete to come before we get the
  // InstallTargetComplete depending on the threading, so check for both.
  if (allcomplete_FullMultipleSecondaries && complete_FullMultipleSecondaries == 2 &&
      manifest_FullMultipleSecondaries) {
    promise_FullMultipleSecondaries.set_value();
  }
}

/*
 * Initialize -> UptaneCycle -> updates downloaded and installed for secondaries
 * without changing the primary.

 * Store installation result for secondary
 */
TEST(Aktualizr, FullMultipleSecondaries) {
  future_FullMultipleSecondaries = promise_FullMultipleSecondaries.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "multisec");
  Config conf("tests/config/basic.toml");
  conf.provision.primary_ecu_serial = "testecuserial";
  conf.provision.primary_ecu_hardware_id = "testecuhwid";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = http->tls_server;
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";

  TemporaryDirectory temp_dir2;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "sec_serial1", "sec_hwid1");
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir2, "sec_serial2", "sec_hwid2");

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FullMultipleSecondaries;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();

  std::future_status status = future_FullMultipleSecondaries.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }

  EXPECT_EQ(started_FullMultipleSecondaries, 2);
  EXPECT_EQ(complete_FullMultipleSecondaries, 2);

  const Json::Value manifest = http->last_manifest["signed"];
  const Json::Value manifest_versions = manifest["ecu_version_manifests"];

  // Make sure filepath were correctly written and formatted.
  EXPECT_EQ(manifest_versions["sec_serial1"]["signed"]["installed_image"]["filepath"].asString(),
            "secondary_firmware.txt");
  EXPECT_EQ(manifest_versions["sec_serial1"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 15);
  EXPECT_EQ(manifest_versions["sec_serial2"]["signed"]["installed_image"]["filepath"].asString(),
            "secondary_firmware2.txt");
  EXPECT_EQ(manifest_versions["sec_serial2"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 21);

  // Make sure there is no result for the primary by checking the size
  EXPECT_EQ(manifest["installation_report"]["report"]["items"].size(), 2);
  EXPECT_EQ(manifest["installation_report"]["report"]["items"][0]["ecu"].asString(), "sec_serial1");
  EXPECT_TRUE(manifest["installation_report"]["report"]["items"][0]["result"]["success"].asBool());
  EXPECT_EQ(manifest["installation_report"]["report"]["items"][1]["ecu"].asString(), "sec_serial2");
  EXPECT_TRUE(manifest["installation_report"]["report"]["items"][1]["result"]["success"].asBool());
}

int num_events_CheckNoUpdates = 0;
std::future<void> future_CheckNoUpdates{};
std::promise<void> promise_CheckNoUpdates{};
void process_events_CheckNoUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->isTypeOf(event::DownloadProgressReport::TypeName)) {
    return;
  }
  LOG_INFO << "Got " << event->variant;
  switch (num_events_CheckNoUpdates) {
    case 0: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
      const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
      EXPECT_EQ(targets_event->result.ecus_count, 0);
      EXPECT_EQ(targets_event->result.updates.size(), 0);
      EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
      break;
    }
    case 1: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
      const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
      EXPECT_EQ(targets_event->result.ecus_count, 0);
      EXPECT_EQ(targets_event->result.updates.size(), 0);
      EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kNoUpdatesAvailable);
      promise_CheckNoUpdates.set_value();
      break;
    }
    case 5:
      // Don't let the test run indefinitely!
      FAIL() << "Unexpected events!";
    default:
      std::cout << "event #" << num_events_CheckNoUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_CheckNoUpdates;
}

/*
 * Initialize -> CheckUpdates -> no updates -> no further action or events.
 */
TEST(Aktualizr, CheckNoUpdates) {
  future_CheckNoUpdates = promise_CheckNoUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_CheckNoUpdates;
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

  std::future_status status = future_CheckNoUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(aktualizr.uptane_client_->AssembleManifest());
}

int num_events_DownloadWithUpdates = 0;
std::future<void> future_DownloadWithUpdates{};
std::promise<void> promise_DownloadWithUpdates{};
void process_events_DownloadWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->isTypeOf(event::DownloadProgressReport::TypeName)) {
    return;
  }
  LOG_INFO << "Got " << event->variant;
  switch (num_events_DownloadWithUpdates) {
    case 0: {
      EXPECT_EQ(event->variant, "AllDownloadsComplete");
      const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
      EXPECT_EQ(downloads_complete->result.updates.size(), 0);
      EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kNothingToDownload);
      break;
    }
    case 1: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
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
      EXPECT_EQ(event->variant, "DownloadTargetComplete");
      const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
      EXPECT_TRUE(download_event->update.filename() == "primary_firmware.txt" ||
                  download_event->update.filename() == "secondary_firmware.txt");
      EXPECT_TRUE(download_event->success);
      break;
    }
    case 4: {
      EXPECT_EQ(event->variant, "AllDownloadsComplete");
      const auto downloads_complete = dynamic_cast<event::AllDownloadsComplete*>(event.get());
      EXPECT_EQ(downloads_complete->result.updates.size(), 2);
      EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "primary_firmware.txt" ||
                  downloads_complete->result.updates[1].filename() == "primary_firmware.txt");
      EXPECT_TRUE(downloads_complete->result.updates[0].filename() == "secondary_firmware.txt" ||
                  downloads_complete->result.updates[1].filename() == "secondary_firmware.txt");
      EXPECT_EQ(downloads_complete->result.status, result::DownloadStatus::kSuccess);
      promise_DownloadWithUpdates.set_value();
      break;
    }
    case 8:
      // Don't let the test run indefinitely!
      FAIL() << "Unexpected events!";
    default:
      std::cout << "event #" << num_events_DownloadWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_DownloadWithUpdates;
}

/*
 * Initialize -> Download -> nothing to download.
 *
 * Initialize -> CheckUpdates -> Download -> updates downloaded but not
 * installed.
 */
TEST(Aktualizr, DownloadWithUpdates) {
  future_DownloadWithUpdates = promise_DownloadWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_DownloadWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  // First try downloading nothing. Nothing should happen.
  result::Download result = aktualizr.Download(std::vector<Uptane::Target>()).get();
  EXPECT_EQ(result.updates.size(), 0);
  EXPECT_EQ(result.status, result::DownloadStatus::kNothingToDownload);
  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  aktualizr.Download(update_result.updates);

  std::future_status status = future_DownloadWithUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for downloads to complete.";
  }

  verifyNothingInstalled(aktualizr.uptane_client_->AssembleManifest());
}

int num_events_InstallWithUpdates = 0;
std::vector<Uptane::Target> updates_InstallWithUpdates;
std::future<void> future_InstallWithUpdates{};
std::promise<void> promise_InstallWithUpdates{};
void process_events_InstallWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  // Note that we do not expect a PutManifestComplete since we don't call
  // UptaneCycle() and that's the only function that generates that.
  if (event->isTypeOf(event::DownloadProgressReport::TypeName)) {
    return;
  }
  LOG_INFO << "Got " << event->variant;
  switch (num_events_InstallWithUpdates) {
    case 0: {
      EXPECT_EQ(event->variant, "AllInstallsComplete");
      const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
      EXPECT_EQ(installs_complete->result.ecu_reports.size(), 0);
      break;
    }
    case 1: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
      const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
      EXPECT_EQ(targets_event->result.ecus_count, 2);
      EXPECT_EQ(targets_event->result.updates.size(), 2u);
      EXPECT_EQ(targets_event->result.updates[0].filename(), "primary_firmware.txt");
      EXPECT_EQ(targets_event->result.updates[1].filename(), "secondary_firmware.txt");
      EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kUpdatesAvailable);
      updates_InstallWithUpdates = targets_event->result.updates;
      break;
    }
    case 2:
    case 3: {
      EXPECT_EQ(event->variant, "DownloadTargetComplete");
      const auto download_event = dynamic_cast<event::DownloadTargetComplete*>(event.get());
      EXPECT_TRUE(download_event->update.filename() == "primary_firmware.txt" ||
                  download_event->update.filename() == "secondary_firmware.txt");
      EXPECT_TRUE(download_event->success);
      break;
    }
    case 4: {
      EXPECT_EQ(event->variant, "AllDownloadsComplete");
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
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "CA:FE:A6:D2:84:9D");
      break;
    }
    case 6: {
      // Primary should complete before secondary begins. (Again not a
      // requirement per se.)
      EXPECT_EQ(event->variant, "InstallTargetComplete");
      const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "CA:FE:A6:D2:84:9D");
      EXPECT_TRUE(install_complete->success);
      break;
    }
    case 7: {
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
      break;
    }
    case 8: {
      EXPECT_EQ(event->variant, "InstallTargetComplete");
      const auto install_complete = dynamic_cast<event::InstallTargetComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
      EXPECT_TRUE(install_complete->success);
      break;
    }
    case 9: {
      EXPECT_EQ(event->variant, "AllInstallsComplete");
      const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
      EXPECT_EQ(installs_complete->result.ecu_reports.size(), 2);
      EXPECT_EQ(installs_complete->result.ecu_reports[0].install_res.result_code.num_code,
                data::ResultCode::Numeric::kOk);
      EXPECT_EQ(installs_complete->result.ecu_reports[1].install_res.result_code.num_code,
                data::ResultCode::Numeric::kOk);
      promise_InstallWithUpdates.set_value();
      break;
    }
    case 12:
      // Don't let the test run indefinitely!
      FAIL() << "Unexpected events!";
    default:
      std::cout << "event #" << num_events_InstallWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_InstallWithUpdates;
}

/*
 * Initialize -> Install -> nothing to install.
 *
 * Initialize -> CheckUpdates -> Download -> Install -> updates installed.
 */
TEST(Aktualizr, InstallWithUpdates) {
  future_InstallWithUpdates = promise_InstallWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_InstallWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  Json::Value primary_json;
  primary_json["hashes"]["sha256"] = "74e653bbf6c00a88b21f0992159b1002f5af38506e6d2fbc7eb9846711b2d75f";
  primary_json["hashes"]["sha512"] =
      "91814ad1c13ebe2af8d65044893633c4c3ce964edb8cb58b0f357406c255f7be94f42547e108b300346a42cd57662e4757b9d843b7acbc09"
      "0df0bc05fe55297f";
  primary_json["length"] = 2;
  Uptane::Target primary_target("primary_firmware.txt", primary_json);

  Json::Value secondary_json;
  secondary_json["hashes"]["sha256"] = "1bbb15aa921ffffd5079567d630f43298dbe5e7cbc1b14e0ccdd6718fde28e47";
  secondary_json["hashes"]["sha512"] =
      "7dbae4c36a2494b731a9239911d3085d53d3e400886edb4ae2b9b78f40bda446649e83ba2d81653f614cc66f5dd5d4dbd95afba854f148af"
      "bfae48d0ff4cc38a";
  secondary_json["length"] = 2;
  Uptane::Target secondary_target("secondary_firmware.txt", secondary_json);

  // First try installing nothing. Nothing should happen.
  result::Install result = aktualizr.Install(updates_InstallWithUpdates).get();
  EXPECT_EQ(result.ecu_reports.size(), 0);

  EXPECT_EQ(aktualizr.GetStoredTarget(primary_target).get(), nullptr)
      << "Primary firmware is present in storage before the download";
  EXPECT_EQ(aktualizr.GetStoredTarget(secondary_target).get(), nullptr)
      << "Secondary firmware is present in storage before the download";

  result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
  aktualizr.Download(update_result.updates).get();
  EXPECT_NE(aktualizr.GetStoredTarget(primary_target).get(), nullptr)
      << "Primary firmware is not present in storage after the download";
  EXPECT_NE(aktualizr.GetStoredTarget(secondary_target).get(), nullptr)
      << "Secondary firmware is not present in storage after the download";

  // After updates have been downloaded, try to install them.
  aktualizr.Install(update_result.updates);

  std::future_status status = future_InstallWithUpdates.wait_for(std::chrono::seconds(20));
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
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);
  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);

  unsigned int report_counter = {0};
  std::shared_ptr<const event::DownloadProgressReport> lastProgressReport{nullptr};

  std::function<void(std::shared_ptr<event::BaseEvent> event)> report_event_hdlr =
      [&](const std::shared_ptr<event::BaseEvent>& event) {
        ASSERT_NE(event, nullptr);
        if (!event->isTypeOf(event::DownloadProgressReport::TypeName)) {
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
  // The test mocks are tailored to emulate a device with a primary ECU and one secondary ECU
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
  HttpFakeCampaign(const boost::filesystem::path& test_dir_in) : HttpFake(test_dir_in) {}

  HttpResponse get(const std::string& url, int64_t maxsize) override {
    EXPECT_NE(url.find("campaigner/"), std::string::npos);
    boost::filesystem::path path = metadata_path.Path() / url.substr(tls_server.size() + strlen("campaigner/"));

    if (url.find("campaigner/campaigns") != std::string::npos) {
      return HttpResponse(Utils::readFile(path.parent_path() / "campaigner/campaigns.json"), 200, CURLE_OK, "");
    }
    return HttpFake::get(url, maxsize);
  }

  HttpResponse handle_event(const std::string& url, const Json::Value& data) override {
    (void)url;
    for (auto it = data.begin(); it != data.end(); it++) {
      auto ev = *it;
      if (ev["eventType"]["id"] == "campaign_accepted") {
        seen_events.push_back(ev);
      }
    }
    return HttpResponse("", 204, CURLE_OK, "");
  }

  std::vector<Json::Value> seen_events;
};

bool campaignaccept_seen = false;
void CampaignCheck_events(const std::shared_ptr<event::BaseEvent>& event) {
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
  }
}

/* Check for campaigns with manual control.
 * Accept a campaign.
 * Fetch campaigns from the server.
 * Send campaign acceptance report.
 * Send CampaignAcceptComplete event.
 * Send CampaignCheckComplete event with campaign data.
 */
TEST(Aktualizr, CampaignCheckAndAccept) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeCampaign>(temp_dir.Path());
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  {
    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);
    std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = CampaignCheck_events;
    aktualizr.SetSignalHandler(f_cb);
    aktualizr.Initialize();

    // check for campaign
    auto result = aktualizr.CampaignCheck().get();
    EXPECT_EQ(result.campaigns.size(), 1);

    // accept the campaign
    aktualizr.CampaignAccept("c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493").get();
  }

  ASSERT_EQ(http->seen_events.size(), 1);
  ASSERT_EQ(http->seen_events[0]["event"]["campaignId"], "c2eb7e8d-8aa0-429d-883f-5ed8fdb2a493");
  ASSERT_TRUE(campaignaccept_seen);
}

class HttpFakeNoCorrelationId : public HttpFake {
 public:
  HttpFakeNoCorrelationId(const boost::filesystem::path& test_dir_in)
      : HttpFake(test_dir_in, "", uptane_repos_dir / "full_no_correlation_id") {}

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

/* Correlation ID is empty if none was provided in targets metadata. */
TEST(Aktualizr, FullNoCorrelationId) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeNoCorrelationId>(temp_dir.Path());

  // scope `Aktualizr` object, so that the ReportQueue flushes its events before
  // we count them at the end
  {
    Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);

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

class CountUpdateCheckEvents {
 public:
  CountUpdateCheckEvents() = default;
  // Non-copyable
  CountUpdateCheckEvents(const CountUpdateCheckEvents&) = delete;
  CountUpdateCheckEvents& operator=(const CountUpdateCheckEvents&) = delete;

  std::function<void(std::shared_ptr<event::BaseEvent>)>& Signal() { return signal_; }

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
  std::function<void(std::shared_ptr<event::BaseEvent>)> signal_{
      [this](std::shared_ptr<event::BaseEvent> e) { this->count(e); }};
};

TEST(Aktualizr, APICheck) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");

  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);

  CountUpdateCheckEvents counter;
  {
    Aktualizr aktualizr(conf, storage, http);
    boost::signals2::connection conn = aktualizr.SetSignalHandler(counter.Signal());
    aktualizr.Initialize();
    for (int i = 0; i < 5; ++i) {
      aktualizr.CheckUpdates();
    }
    std::this_thread::sleep_for(std::chrono::seconds(12));
    // Wait for the Aktualizr dtor to run in order that processing has finished
  }

  EXPECT_EQ(counter.total_events(), 5);

  // try again, but shutdown before it finished all calls
  CountUpdateCheckEvents counter2;
  {
    Aktualizr aktualizr(conf, storage, http);
    boost::signals2::connection conn = aktualizr.SetSignalHandler(counter2.Signal());
    aktualizr.Initialize();
    for (int i = 0; i < 100; ++i) {
      aktualizr.CheckUpdates();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    // Wait for the Aktualizr dtor to run in order that processing has finished
  }
  EXPECT_LT(counter2.total_events(), 100);
  EXPECT_GT(counter2.total_events(), 0);
}

class HttpPutManifestFail : public HttpFake {
 public:
  HttpPutManifestFail(const boost::filesystem::path& test_dir_in) : HttpFake(test_dir_in) {}
  HttpResponse put(const std::string& url, const Json::Value& data) override {
    (void)data;
    return HttpResponse(url, 504, CURLE_OK, "");
  }
};

/* Send UpdateCheckComplete event after failure */
TEST(Aktualizr, UpdateCheckCompleteError) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpPutManifestFail>(temp_dir.Path());

  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, "http://updatefail");

  auto storage = INvStorage::newStorage(conf.storage);
  CountUpdateCheckEvents counter;

  Aktualizr aktualizr(conf, storage, http);
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
  HttpFakePauseCounter(const boost::filesystem::path& test_dir_in) : HttpFake(test_dir_in, "noupdates") {}

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
  auto http = std::make_shared<HttpFakePauseCounter>(temp_dir.Path());
  Config conf = UptaneTestCommon::makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  aktualizr.Initialize();

  std::mutex mutex;
  std::promise<void> end_promise{};
  size_t n_events = 0;
  std::atomic_bool is_paused{false};
  std::function<void(std::shared_ptr<event::BaseEvent>)> cb = [&end_promise, &n_events, &mutex,
                                                               &is_paused](std::shared_ptr<event::BaseEvent> event) {
    switch (n_events) {
      case 0:
        EXPECT_EQ(event->variant, "UpdateCheckComplete");
        break;
      case 1: {
        std::lock_guard<std::mutex> guard(mutex);

        EXPECT_EQ(event->variant, "UpdateCheckComplete");
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

  std::future_status status = end_promise.get_future().wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for UpdateCheck event";
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the base directory of uptane repos.\n";
    return EXIT_FAILURE;
  }
  uptane_repos_dir = argv[1];

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
