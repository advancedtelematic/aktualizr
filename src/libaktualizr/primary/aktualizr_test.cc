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
#include "uptane_test_common.h"
#include "utilities/utils.h"

boost::filesystem::path uptane_repos_dir;

Config makeTestConfig(const TemporaryDirectory& temp_dir, const std::string& url) {
  Config conf("tests/config/basic.toml");
  conf.uptane.director_server = url + "/director";
  conf.uptane.repo_server = url + "/repo";
  conf.provision.server = url;
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = url;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");
  return conf;
}

void verifyNothingInstalled(const Json::Value& manifest) {
  // Verify nothing has installed for the primary.
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["id"].asString(), "");
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_code"].asInt(),
            static_cast<int>(data::UpdateResultCode::kOk));
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_text"].asString(), "");
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"]["filepath"].asString(), "unknown");
  // Verify nothing has installed for the secondary.
  EXPECT_EQ(manifest["secondary_ecu_serial"]["signed"]["installed_image"]["filepath"].asString(), "noimage");
}

int num_events_FullNoUpdates = 0;
std::future<void> future_FullNoUpdates{};
std::promise<void> promise_FullNoUpdates{};
void process_events_FullNoUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadProgressReport") {
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
 * Initialize -> CheckUpdates -> no updates -> no further action or events.
 */
TEST(Aktualizr, FullNoUpdates) {
  future_FullNoUpdates = promise_FullNoUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates");
  Config conf = makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FullNoUpdates;
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

  std::future_status status = future_FullNoUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(aktualizr.uptane_client_->AssembleManifest());
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
  if (event->variant == "DownloadProgressReport") {
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
      EXPECT_EQ(installs_complete->result.reports.size(), 2);
      EXPECT_EQ(installs_complete->result.reports[0].status.result_code, data::UpdateResultCode::kOk);
      EXPECT_EQ(installs_complete->result.reports[1].status.result_code, data::UpdateResultCode::kOk);
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
  Config conf = makeTestConfig(temp_dir, http->tls_server);

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
  Config conf = makeTestConfig(temp_dir, http->tls_server);
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
  const Json::Value manifest = aktualizr.uptane_client_->AssembleManifest();
  // Make sure filepath were correctly written and formatted.
  // installation_result has not been implemented for secondaries yet.
  EXPECT_EQ(manifest["sec_serial1"]["signed"]["installed_image"]["filepath"].asString(), "secondary_firmware.txt");
  EXPECT_EQ(manifest["sec_serial1"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 15);
  EXPECT_EQ(manifest["sec_serial2"]["signed"]["installed_image"]["filepath"].asString(), "secondary_firmware2.txt");
  EXPECT_EQ(manifest["sec_serial2"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 21);
  // Manifest should not have an installation result for the primary.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
}

int num_events_CheckWithUpdates = 0;
std::future<void> future_CheckWithUpdates{};
std::promise<void> promise_CheckWithUpdates{};
void process_events_CheckWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  switch (num_events_CheckWithUpdates) {
    case 0: {
      EXPECT_EQ(event->variant, "UpdateCheckComplete");
      const auto targets_event = dynamic_cast<event::UpdateCheckComplete*>(event.get());
      EXPECT_EQ(targets_event->result.ecus_count, 2);
      EXPECT_EQ(targets_event->result.updates.size(), 2u);
      EXPECT_EQ(targets_event->result.updates[0].filename(), "primary_firmware.txt");
      EXPECT_EQ(targets_event->result.updates[1].filename(), "secondary_firmware.txt");
      EXPECT_EQ(targets_event->result.status, result::UpdateStatus::kUpdatesAvailable);
      promise_CheckWithUpdates.set_value();
      break;
    }
    case 5:
      // Don't let the test run indefinitely!
      FAIL() << "Unexpected events!";
    default:
      std::cout << "event #" << num_events_CheckWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_CheckWithUpdates;
}

/*
 * Initialize -> CheckUpdates -> updates found but not downloaded.
 */
TEST(Aktualizr, CheckWithUpdates) {
  future_CheckWithUpdates = promise_CheckWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config conf = makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_CheckWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.CheckUpdates();
  std::future_status status = future_CheckWithUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(aktualizr.uptane_client_->AssembleManifest());
}

int num_events_DownloadWithUpdates = 0;
std::future<void> future_DownloadWithUpdates{};
std::promise<void> promise_DownloadWithUpdates{};
void process_events_DownloadWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadProgressReport") {
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
  Config conf = makeTestConfig(temp_dir, http->tls_server);

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
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  LOG_INFO << "Got " << event->variant;
  switch (num_events_InstallWithUpdates) {
    case 0: {
      EXPECT_EQ(event->variant, "AllInstallsComplete");
      const auto installs_complete = dynamic_cast<event::AllInstallsComplete*>(event.get());
      EXPECT_EQ(installs_complete->result.reports.size(), 0);
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
      EXPECT_EQ(installs_complete->result.reports.size(), 2);
      EXPECT_EQ(installs_complete->result.reports[0].status.result_code, data::UpdateResultCode::kOk);
      EXPECT_EQ(installs_complete->result.reports[1].status.result_code, data::UpdateResultCode::kOk);
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
  Config conf = makeTestConfig(temp_dir, http->tls_server);

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
  EXPECT_EQ(result.reports.size(), 0);

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
};

/* Check for campaigns with manual control.
 * Fetch campaigns from the server. */
TEST(Aktualizr, CampaignCheck) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeCampaign>(temp_dir.Path());
  Config conf = makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);

  aktualizr.Initialize();
  auto result = aktualizr.CampaignCheck().get();
  EXPECT_EQ(result.campaigns.size(), 1);
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
    Config conf = makeTestConfig(temp_dir, http->tls_server);

    auto storage = INvStorage::newStorage(conf.storage);
    Aktualizr aktualizr(conf, storage, http);

    aktualizr.Initialize();
    result::UpdateCheck update_result = aktualizr.CheckUpdates().get();
    EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

    result::Download download_result = aktualizr.Download(update_result.updates).get();
    EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

    result::Install install_result = aktualizr.Install(download_result.updates).get();
    for (const auto& r : install_result.reports) {
      EXPECT_EQ(r.status.result_code, data::UpdateResultCode::kOk);
    }
  }

  EXPECT_EQ(http->events_seen, 8);
}

int num_events_UpdateCheck = 0;
void process_events_UpdateCheck(const std::shared_ptr<event::BaseEvent>& event) {
  std::cout << event->variant << "\n";
  if (event->variant == "UpdateCheckComplete") {
    num_events_UpdateCheck++;
  }
}

TEST(Aktualizr, APICheck) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");

  Config conf = makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_UpdateCheck;

  {
    Aktualizr aktualizr(conf, storage, http);
    boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);
    aktualizr.Initialize();
    for (int i = 0; i < 5; ++i) {
      aktualizr.CheckUpdates();
    }
    std::this_thread::sleep_for(std::chrono::seconds(12));
    aktualizr.Shutdown();
  }

  EXPECT_EQ(num_events_UpdateCheck, 5);

  num_events_UpdateCheck = 0;
  // try again, but shutdown before it finished all calls
  {
    Aktualizr aktualizr(conf, storage, http);
    boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);
    aktualizr.Initialize();
    for (int i = 0; i < 100; ++i) {
      aktualizr.CheckUpdates();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    aktualizr.Shutdown();
  }
  EXPECT_LT(num_events_UpdateCheck, 100);
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
TEST(Aktualizr, PutManifestError) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpPutManifestFail>(temp_dir.Path());

  Config conf = makeTestConfig(temp_dir, "http://putmanifesterror");

  auto storage = INvStorage::newStorage(conf.storage);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_UpdateCheck;

  num_events_UpdateCheck = 0;
  Aktualizr aktualizr(conf, storage, http);
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);
  aktualizr.Initialize();
  auto result = aktualizr.CheckUpdates().get();
  EXPECT_EQ(result.status, result::UpdateStatus::kError);
  EXPECT_EQ(num_events_UpdateCheck, 1);
}

/* Test that Aktualizr retransmits DownloadPaused and DownloadResumed events */
TEST(Aktualizr, PauseResumeEvents) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "noupdates");
  Config conf = makeTestConfig(temp_dir, http->tls_server);

  auto storage = INvStorage::newStorage(conf.storage);
  Aktualizr aktualizr(conf, storage, http);

  std::promise<void> end_promise{};
  size_t n_events = 0;
  std::function<void(std::shared_ptr<event::BaseEvent>)> cb = [&end_promise,
                                                               &n_events](std::shared_ptr<event::BaseEvent> event) {
    switch (n_events) {
      case 0:
        EXPECT_EQ(event->variant, "DownloadPaused");
        break;
      case 1:
        EXPECT_EQ(event->variant, "DownloadResumed");
        end_promise.set_value();
        break;
      default:
        FAIL() << "Unexpected event";
    }
    n_events += 1;
  };
  boost::signals2::connection conn = aktualizr.SetSignalHandler(cb);

  aktualizr.Pause();
  aktualizr.Resume();

  std::future_status status = end_promise.get_future().wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for pause/resume events";
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
