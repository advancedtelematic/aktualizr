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
#include "primary/sotauptaneclient.h"
#include "uptane_test_common.h"
#include "utilities/events.h"
#include "utilities/utils.h"

Config makeTestConfig(const TemporaryDirectory& temp_dir, const std::string& url) {
  Config conf("tests/config/basic.toml");
  conf.uptane.director_server = url + "/director";
  conf.uptane.repo_server = url + "/repo";
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = BasedPath("metadata");
  conf.storage.uptane_private_key_path = BasedPath("private.key");
  conf.storage.uptane_public_key_path = BasedPath("public.key");
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
  switch (num_events_FullNoUpdates) {
    case 0:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 1:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      promise_FullNoUpdates.set_value();
      break;
    case 5:
      // Don't let the test run indefinitely!
      FAIL();
    default:
      std::cout << "event #" << num_events_FullNoUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_FullNoUpdates;
}

/*
 * Using automatic control, test that if there are no updates, no additional
 * events are generated beyond the expected ones.
 */
TEST(Aktualizr, FullNoUpdates) {
  future_FullNoUpdates = promise_FullNoUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf = makeTestConfig(temp_dir, http->tls_server);
  conf.uptane.director_server = http->tls_server + "/noupdates/director";
  conf.uptane.repo_server = http->tls_server + "/noupdates/repo";
  conf.uptane.running_mode = RunningMode::kFull;

  auto storage = INvStorage::newStorage(conf.storage);
  auto sig = std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  Aktualizr aktualizr(conf, storage, up, sig);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FullNoUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.FetchMetadata();
  // Fetch twice so that we can check for a second FetchMetaComplete and
  // guarantee that nothing unexpected happened after the first fetch.
  aktualizr.FetchMetadata();

  std::future_status status = future_FullNoUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(up->AssembleManifest());
}

int num_events_FullWithUpdates = 0;
int num_complete_FullWithUpdates = 0;
std::future<void> future_FullWithUpdates{};
std::promise<void> promise_FullWithUpdates{};
void process_events_FullWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  LOG_INFO << "Got " << event->variant;
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  switch (num_events_FullWithUpdates) {
    case 0:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 1: {
      EXPECT_EQ(event->variant, "UpdateAvailable");
      const auto targets_event = dynamic_cast<event::UpdateAvailable*>(event.get());
      EXPECT_EQ(targets_event->updates.size(), 2u);
      EXPECT_EQ(targets_event->updates[0].filename(), "primary_firmware.txt");
      EXPECT_EQ(targets_event->updates[1].filename(), "secondary_firmware.txt");
      break;
    }
    case 2:
      EXPECT_EQ(event->variant, "DownloadComplete");
      break;
    case 3: {
      // Primary always gets installed first. (Not a requirement, just how it
      // works at present.)
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "CA:FE:A6:D2:84:9D");
      break;
    }
    case 4: {
      // Primary should complete before secondary begins. (Again not a
      // requirement per se.)
      EXPECT_EQ(event->variant, "InstallComplete");
      const auto install_complete = dynamic_cast<event::InstallComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "CA:FE:A6:D2:84:9D");
      ++num_complete_FullWithUpdates;
      break;
    }
    case 5: {
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
      break;
    }
    case 6:
      EXPECT_EQ(event->variant, "InstallComplete");
      ++num_complete_FullWithUpdates;
      EXPECT_EQ(num_complete_FullWithUpdates, 2);
      {
        const auto install_complete = dynamic_cast<event::InstallComplete*>(event.get());
        EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
      }
      break;
    case 7:
      EXPECT_EQ(event->variant, "AllInstallsComplete");
      break;
    case 8:
      EXPECT_EQ(event->variant, "PutManifestComplete");
      promise_FullWithUpdates.set_value();
      break;
    case 10:
      // Don't let the test run indefinitely!
      FAIL();
    default:
      std::cout << "event #" << num_events_FullWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_FullWithUpdates;
}

/*
 * Using automatic control, test that pre-made updates are successfully
 * downloaded and installed for both primary and secondary. Verify the exact
 * sequence of expected events.
 */
TEST(Aktualizr, FullWithUpdates) {
  future_FullWithUpdates = promise_FullWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf = makeTestConfig(temp_dir, http->tls_server);
  conf.uptane.running_mode = RunningMode::kFull;

  auto storage = INvStorage::newStorage(conf.storage);
  auto sig = std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  Aktualizr aktualizr(conf, storage, up, sig);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FullWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();

  std::future_status status = future_FullWithUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
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
  } else if (event->variant == "InstallComplete") {
    ++complete_FullMultipleSecondaries;
  } else if (event->variant == "AllInstallsComplete") {
    allcomplete_FullMultipleSecondaries = true;
  } else if (event->variant == "PutManifestComplete") {
    manifest_FullMultipleSecondaries = true;
  }
  // It is possible for the PutManifestComplete to come before we get the
  // InstallComplete depending on the threading, so check for both.
  if (allcomplete_FullMultipleSecondaries && complete_FullMultipleSecondaries == 2 &&
      manifest_FullMultipleSecondaries) {
    promise_FullMultipleSecondaries.set_value();
  }
}

/*
 * Verify successful installation of multiple secondaries without updating the
 * primary.
 */
TEST(Aktualizr, FullMultipleSecondaries) {
  future_FullMultipleSecondaries = promise_FullMultipleSecondaries.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf("tests/config/basic.toml");
  conf.provision.primary_ecu_serial = "testecuserial";
  conf.provision.primary_ecu_hardware_id = "testecuhwid";
  conf.uptane.director_server = http->tls_server + "/multisec/director";
  conf.uptane.repo_server = http->tls_server + "/multisec/repo";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_private_key_path = BasedPath("private.key");
  conf.storage.uptane_public_key_path = BasedPath("public.key");
  conf.tls.server = http->tls_server;
  conf.uptane.running_mode = RunningMode::kFull;

  TemporaryDirectory temp_dir2;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "sec_serial1", "sec_hwid1");
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir2, "sec_serial2", "sec_hwid2");

  auto storage = INvStorage::newStorage(conf.storage);
  auto sig = std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  Aktualizr aktualizr(conf, storage, up, sig);
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
  const Json::Value manifest = up->AssembleManifest();
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
    case 0:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 1: {
      EXPECT_EQ(event->variant, "UpdateAvailable");
      const auto targets_event = dynamic_cast<event::UpdateAvailable*>(event.get());
      EXPECT_EQ(targets_event->updates.size(), 2u);
      EXPECT_EQ(targets_event->updates[0].filename(), "primary_firmware.txt");
      EXPECT_EQ(targets_event->updates[1].filename(), "secondary_firmware.txt");
      promise_CheckWithUpdates.set_value();
      break;
    }
    case 5:
      // Don't let the test run indefinitely!
      FAIL();
    default:
      std::cout << "event #" << num_events_CheckWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_CheckWithUpdates;
}

/*
 * If only checking for updates, we shouldn't download or install anything.
 */
TEST(Aktualizr, CheckWithUpdates) {
  future_CheckWithUpdates = promise_CheckWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf = makeTestConfig(temp_dir, http->tls_server);
  conf.uptane.running_mode = RunningMode::kCheck;

  auto storage = INvStorage::newStorage(conf.storage);
  auto sig = std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  Aktualizr aktualizr(conf, storage, up, sig);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_CheckWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  aktualizr.UptaneCycle();

  std::future_status status = future_CheckWithUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for metadata to be fetched.";
  }

  verifyNothingInstalled(up->AssembleManifest());
}

int num_events_DownloadWithUpdates = 0;
std::future<void> future_DownloadWithUpdates{};
std::promise<void> promise_DownloadWithUpdates{};
void process_events_DownloadWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  switch (num_events_DownloadWithUpdates) {
    case 0:
      EXPECT_EQ(event->variant, "NothingToDownload");
      break;
    case 1:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 2: {
      EXPECT_EQ(event->variant, "UpdateAvailable");
      const auto targets_event = dynamic_cast<event::UpdateAvailable*>(event.get());
      EXPECT_EQ(targets_event->updates.size(), 2u);
      EXPECT_EQ(targets_event->updates[0].filename(), "primary_firmware.txt");
      EXPECT_EQ(targets_event->updates[1].filename(), "secondary_firmware.txt");
      break;
    }
    case 3:
      EXPECT_EQ(event->variant, "DownloadComplete");
      promise_DownloadWithUpdates.set_value();
      break;
    case 6:
      // Don't let the test run indefinitely!
      FAIL();
    default:
      std::cout << "event #" << num_events_DownloadWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_DownloadWithUpdates;
}

/*
 * If only downloading, we shouldn't install anything. Start with a fetch, since
 * download can't be called without a vector of updates.
 */
TEST(Aktualizr, DownloadWithUpdates) {
  future_DownloadWithUpdates = promise_DownloadWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf = makeTestConfig(temp_dir, http->tls_server);
  conf.uptane.running_mode = RunningMode::kDownload;

  auto storage = INvStorage::newStorage(conf.storage);
  auto sig = std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  Aktualizr aktualizr(conf, storage, up, sig);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_DownloadWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  // First try downloading nothing. Nothing should happen.
  aktualizr.Download(std::vector<Uptane::Target>());
  aktualizr.UptaneCycle();

  std::future_status status = future_DownloadWithUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for downloads to complete.";
  }

  verifyNothingInstalled(up->AssembleManifest());
}

int num_events_InstallWithUpdates = 0;
std::vector<Uptane::Target> updates_InstallWithUpdates;
std::future<void> future_InstallWithUpdates{};
std::promise<void> promise_InstallWithUpdates{};
void process_events_InstallWithUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  // Note that we do not expect a PutManifestComplete since we are not using the
  // kFull or kOnce running modes.
  std::cout << "Got " << event->variant << " event\n";
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  switch (num_events_InstallWithUpdates) {
    case 0:
      EXPECT_EQ(event->variant, "AllInstallsComplete");
      break;
    case 1:
    case 4:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 2:
    case 5: {
      EXPECT_EQ(event->variant, "UpdateAvailable");
      const auto targets_event = dynamic_cast<event::UpdateAvailable*>(event.get());
      EXPECT_EQ(targets_event->updates.size(), 2u);
      EXPECT_EQ(targets_event->updates[0].filename(), "primary_firmware.txt");
      EXPECT_EQ(targets_event->updates[1].filename(), "secondary_firmware.txt");
      updates_InstallWithUpdates = targets_event->updates;
      break;
    }
    case 3:
      EXPECT_EQ(event->variant, "DownloadComplete");
      break;
    case 6: {
      // Primary always gets installed first. (Not a requirement, just how it
      // works at present.)
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "CA:FE:A6:D2:84:9D");
      break;
    }
    case 7: {
      // Primary should complete before secondary begins. (Again not a
      // requirement per se.)
      EXPECT_EQ(event->variant, "InstallComplete");
      const auto install_complete = dynamic_cast<event::InstallComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "CA:FE:A6:D2:84:9D");
      break;
    }
    case 8: {
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
      break;
    }
    case 9: {
      EXPECT_EQ(event->variant, "InstallComplete");
      const auto install_complete = dynamic_cast<event::InstallComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
      break;
    }
    case 10: {
      EXPECT_EQ(event->variant, "AllInstallsComplete");
      break;
    }
    case 11: {
      EXPECT_EQ(event->variant, "PutManifestComplete");
      promise_InstallWithUpdates.set_value();
      break;
    }
    case 13:
      // Don't let the test run indefinitely!
      FAIL();
    default:
      std::cout << "event #" << num_events_InstallWithUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_InstallWithUpdates;
}

/*
 * After updates have been downloaded, try to install them. For the download and
 * the install, rely on the fetch to automatically do the right thing.
 */
TEST(Aktualizr, InstallWithUpdates) {
  future_InstallWithUpdates = promise_InstallWithUpdates.get_future();
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf = makeTestConfig(temp_dir, http->tls_server);
  conf.uptane.running_mode = RunningMode::kInstall;

  auto storage = INvStorage::newStorage(conf.storage);
  auto sig = std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  Aktualizr aktualizr(conf, storage, up, sig);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_InstallWithUpdates;
  boost::signals2::connection conn = aktualizr.SetSignalHandler(f_cb);

  aktualizr.Initialize();
  // First try installing nothing. Nothing should happen.
  aktualizr.Install(updates_InstallWithUpdates);
  conf.uptane.running_mode = RunningMode::kDownload;
  aktualizr.UptaneCycle();
  conf.uptane.running_mode = RunningMode::kInstall;
  aktualizr.UptaneCycle();

  std::future_status status = future_InstallWithUpdates.wait_for(std::chrono::seconds(20));
  if (status != std::future_status::ready) {
    FAIL() << "Timed out waiting for installation to complete.";
  }
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
