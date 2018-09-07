#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "config/config.h"
#include "httpfake.h"
// #include "primary/aktualizr.h"
#include "primary/sotauptaneclient.h"
#include "uptane_test_common.h"
#include "utilities/events.h"
#include "utilities/utils.h"

boost::filesystem::path sysroot;

int num_events_FetchNoUpdates = 0;
void process_events_FetchNoUpdates(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  switch (num_events_FetchNoUpdates) {
    case 0:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 1:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 5:
      // Don't let the test run indefinitely!
      FAIL();
    default:
      std::cout << "event #" << num_events_FetchNoUpdates << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_FetchNoUpdates;
}

/*
 * Using automatic control, test that if there are no updates, no additional
 * events are generated beyond the expected ones.
 */
TEST(Uptane, FetchNoUpdates) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf("tests/config/basic.toml");
  conf.uptane.director_server = http->tls_server + "/noupdates/director";
  conf.uptane.repo_server = http->tls_server + "/noupdates/repo";
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.uptane.running_mode = RunningMode::kFull;
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = BasedPath("metadata");
  conf.storage.uptane_private_key_path = BasedPath("private.key");
  conf.storage.uptane_public_key_path = BasedPath("public.key");
  conf.pacman.sysroot = sysroot;
  conf.tls.server = http->tls_server;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");

  std::shared_ptr<event::Channel> sig =
      std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FetchNoUpdates;
  sig->connect(f_cb);

  auto storage = INvStorage::newStorage(conf.storage);
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  EXPECT_NO_THROW(up->initialize());
  up->fetchMeta();
  // Fetch twice so that we can check for a second FetchMetaComplete and
  // guarantee that nothing unexpected happened after the first fetch.
  up->fetchMeta();

  size_t counter = 0;
  while (num_events_FetchNoUpdates < 2) {
    sleep(1);
    ASSERT_LT(++counter, 20) << "Timed out waiting for metadata to be fetched.";
  }

  Json::Value manifest = up->AssembleManifest();
  // Verify nothing has installed for the primary.
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["id"].asString(), "");
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_code"].asInt(),
            static_cast<int>(data::UpdateResultCode::kOk));
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_text"].asString(), "");
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"]["filepath"].asString(), "unknown");
  // Verify nothing has installed for the secondary.
  EXPECT_EQ(manifest["secondary_ecu_serial"]["signed"]["installed_image"]["filepath"].asString(), "noimage");
}

int num_events_FetchDownloadInstall = 0;
void process_events_FetchDownloadInstall(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "DownloadProgressReport") {
    return;
  }
  switch (num_events_FetchDownloadInstall) {
    case 0:
      EXPECT_EQ(event->variant, "FetchMetaComplete");
      break;
    case 1: {
      EXPECT_EQ(event->variant, "UpdateAvailable");
      auto targets_event = dynamic_cast<event::UpdateAvailable*>(event.get());
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
      break;
    }
    case 5: {
      EXPECT_EQ(event->variant, "InstallStarted");
      const auto install_started = dynamic_cast<event::InstallStarted*>(event.get());
      EXPECT_EQ(install_started->serial.ToString(), "secondary_ecu_serial");
      break;
    }
    case 6: {
      EXPECT_EQ(event->variant, "InstallComplete");
      const auto install_complete = dynamic_cast<event::InstallComplete*>(event.get());
      EXPECT_EQ(install_complete->serial.ToString(), "secondary_ecu_serial");
      break;
    }
    case 7:
      EXPECT_EQ(event->variant, "PutManifestComplete");
      break;
    case 10:
      // Don't let the test run indefinitely!
      FAIL();
    default:
      std::cout << "event #" << num_events_FetchDownloadInstall << " is: " << event->variant << "\n";
      EXPECT_EQ(event->variant, "");
  }
  ++num_events_FetchDownloadInstall;
}

/*
 * Using automatic control, test that pre-made updates are successfully
 * downloaded and installed for both primary and secondary. Verify the exact
 * sequence of expected events.
 */
TEST(Uptane, FetchDownloadInstall) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config conf("tests/config/basic.toml");
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.uptane.running_mode = RunningMode::kFull;
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = BasedPath("metadata");
  conf.storage.uptane_private_key_path = BasedPath("private.key");
  conf.storage.uptane_public_key_path = BasedPath("public.key");
  conf.pacman.sysroot = sysroot;
  conf.tls.server = http->tls_server;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");

  std::shared_ptr<event::Channel> sig =
      std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_FetchDownloadInstall;
  sig->connect(f_cb);

  auto storage = INvStorage::newStorage(conf.storage);
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  EXPECT_NO_THROW(up->initialize());
  up->fetchMeta();

  size_t counter = 0;
  while (num_events_FetchDownloadInstall < 8) {
    sleep(1);
    ASSERT_LT(++counter, 20) << "Timed out waiting for primary installation to complete.";
  }

  Json::Value manifest = up->AssembleManifest();
  // Make sure operation_result and filepath were correctly written and formatted.
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["id"].asString(),
            "primary_firmware.txt");
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_code"].asInt(),
            static_cast<int>(data::UpdateResultCode::kOk));
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["custom"]["operation_result"]["result_text"].asString(),
            "Installing fake package was successful");
  EXPECT_EQ(manifest["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"]["filepath"].asString(), "primary_firmware.txt");
  // installation_result has not been implemented for secondaries yet.
  EXPECT_EQ(manifest["secondary_ecu_serial"]["signed"]["installed_image"]["filepath"].asString(),
            "secondary_firmware.txt");
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to an OSTree sysroot as an input argument.\n";
    return EXIT_FAILURE;
  }
  sysroot = argv[1];
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
