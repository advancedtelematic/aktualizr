#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include "crypto/p11engine.h"
#include "httpfake.h"
#include "primary/initializer.h"
#include "primary/sotauptaneclient.h"
#include "storage/fsstorage_read.h"
#include "storage/invstorage.h"
#include "test_utils.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

#ifdef BUILD_P11
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

boost::filesystem::path sysroot;

Uptane::SecondaryConfig addDefaultSecondary(Config& config, const TemporaryDirectory& temp_dir,
                                            const std::string& serial, const std::string& hw_id) {
  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::SecondaryType::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path();
  ecu_config.ecu_serial = serial;
  ecu_config.ecu_hardware_id = hw_id;
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir / "firmware.txt";
  ecu_config.target_name_path = temp_dir / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir / "secondary_metadata";
  config.uptane.secondary_configs.push_back(ecu_config);
  return ecu_config;
}

std::vector<Uptane::Target> makePackage(const std::string& serial, const std::string& hw_id) {
  std::vector<Uptane::Target> packages_to_install;
  Json::Value ot_json;
  ot_json["custom"]["ecuIdentifiers"][serial]["hardwareId"] = hw_id;
  ot_json["custom"]["targetFormat"] = "OSTREE";
  ot_json["length"] = 0;
  ot_json["hashes"]["sha256"] = serial;
  packages_to_install.emplace_back(serial, ot_json);
  return packages_to_install;
}

TEST(Uptane, Verify) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  HttpResponse response = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit);
  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  Uptane::Root(Uptane::RepositoryType::Director, response.getJson(), root);
}

TEST(Uptane, VerifyDataBad) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();
  data_json.removeMember("signatures");

  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director, data_json, root), Uptane::UnmetThreshold);
}

TEST(Uptane, VerifyDataUnknownType) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();
  data_json["signatures"][0]["method"] = "badsignature";
  data_json["signatures"][1]["method"] = "badsignature";

  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director, data_json, root), Uptane::SecurityException);
}

TEST(Uptane, VerifyDataBadKeyId) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();

  data_json["signatures"][0]["keyid"] = "badkeyid";

  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director, data_json, root), Uptane::BadKeyId);
}

TEST(Uptane, VerifyDataBadThreshold) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();
  data_json["signed"]["roles"]["root"]["threshold"] = -1;
  try {
    Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
    Uptane::Root(Uptane::RepositoryType::Director, data_json, root);
    FAIL();
  } catch (const Uptane::IllegalThreshold& ex) {
  } catch (const Uptane::UnmetThreshold& ex) {
  }
}

TEST(Uptane, AssembleManifestGood) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = BasedPath("metadata");
  config.storage.uptane_private_key_path = BasedPath("private.key");
  config.storage.uptane_public_key_path = BasedPath("public.key");
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PackageManager::kNone;
  addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  Json::Value manifest = sota_client->AssembleManifest();
  EXPECT_EQ(manifest.size(), 2);
  EXPECT_EQ(manifest["testecuserial"]["signed"]["ecu_serial"].asString(), config.provision.primary_ecu_serial);
  EXPECT_EQ(manifest["secondary_ecu_serial"]["signed"]["ecu_serial"].asString(), "secondary_ecu_serial");
  // Manifest should not have an installation result yet.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
  EXPECT_FALSE(manifest["secondary_ecu_serial"]["signed"].isMember("custom"));
}

TEST(Uptane, AssembleManifestBad) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = BasedPath("metadata");
  config.storage.uptane_private_key_path = BasedPath("private.key");
  config.storage.uptane_public_key_path = BasedPath("public.key");
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PackageManager::kNone;
  Uptane::SecondaryConfig ecu_config =
      addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  std::string private_key, public_key;
  Crypto::generateKeyPair(ecu_config.key_type, &public_key, &private_key);
  Utils::writeFile(ecu_config.full_client_dir / ecu_config.ecu_private_key, private_key);
  public_key = Utils::readFile("tests/test_data/public.key");
  Utils::writeFile(ecu_config.full_client_dir / ecu_config.ecu_public_key, public_key);

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  Json::Value manifest = sota_client->AssembleManifest();
  EXPECT_EQ(manifest.size(), 1);
  EXPECT_EQ(manifest["testecuserial"]["signed"]["ecu_serial"].asString(), config.provision.primary_ecu_serial);
  // Manifest should not have an installation result yet.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
  EXPECT_FALSE(manifest["secondary_ecu_serial"]["signed"].isMember("custom"));
}

TEST(Uptane, PutManifest) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = BasedPath("metadata");
  config.storage.uptane_private_key_path = BasedPath("private.key");
  config.storage.uptane_public_key_path = BasedPath("public.key");
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PackageManager::kNone;
  addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);

  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());
  EXPECT_TRUE(sota_client->putManifest());

  EXPECT_TRUE(boost::filesystem::exists(temp_dir / http->test_manifest));
  Json::Value json = Utils::parseJSONFile((temp_dir / http->test_manifest).string());

  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(
      json["signed"]["ecu_version_manifests"]["testecuserial"]["signed"]["installed_image"]["filepath"].asString(),
      "unknown");
  EXPECT_EQ(json["signed"]["ecu_version_manifests"].size(), 2u);
  EXPECT_EQ(json["signed"]["ecu_version_manifests"]["secondary_ecu_serial"]["signed"]["ecu_serial"].asString(),
            "secondary_ecu_serial");
  EXPECT_EQ(json["signed"]["ecu_version_manifests"]["secondary_ecu_serial"]["signed"]["installed_image"]["filepath"]
                .asString(),
            "test-package");
}

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
  addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");

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
  addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");

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

/*
 * Verify successful installation of a provided fake package. Skip fetching and
 * downloading.
 */
TEST(Uptane, InstallOnly) {
  Config conf("tests/config/basic.toml");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  conf.provision.primary_ecu_serial = "testecuserial";
  conf.provision.primary_ecu_hardware_id = "testecuhwid";
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_private_key_path = BasedPath("private.key");
  conf.storage.uptane_public_key_path = BasedPath("public.key");
  conf.pacman.sysroot = sysroot;
  conf.tls.server = http->tls_server;

  auto storage = INvStorage::newStorage(conf.storage);
  auto up = SotaUptaneClient::newTestClient(conf, storage, http);
  EXPECT_NO_THROW(up->initialize());
  std::vector<Uptane::Target> packages_to_install = makePackage("testecuserial", "testecuhwid");
  up->uptaneInstall(packages_to_install);

  // Make sure operation_result and filepath were correctly written and formatted.
  Json::Value manifest = up->AssembleManifest();
  EXPECT_EQ(manifest["testecuserial"]["signed"]["custom"]["operation_result"]["id"].asString(), "testecuserial");
  EXPECT_EQ(manifest["testecuserial"]["signed"]["custom"]["operation_result"]["result_code"].asInt(),
            static_cast<int>(data::UpdateResultCode::kOk));
  EXPECT_EQ(manifest["testecuserial"]["signed"]["custom"]["operation_result"]["result_text"].asString(),
            "Installing fake package was successful");
  EXPECT_EQ(manifest["testecuserial"]["signed"]["installed_image"]["filepath"].asString(), "testecuserial");
  // Verify nothing has installed for the secondary.
  EXPECT_FALSE(manifest["secondary_ecu_serial"]["signed"].isMember("installed_image"));
}

TEST(Uptane, UptaneSecondaryAdd) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.uptane.repo_server = http->tls_server + "/director";
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.uptane.repo_server = http->tls_server + "/repo";
  config.tls.server = http->tls_server;
  config.provision.primary_ecu_serial = "testecuserial";
  config.storage.path = temp_dir.Path();
  config.storage.uptane_private_key_path = BasedPath("private.key");
  config.storage.uptane_public_key_path = BasedPath("public.key");
  config.pacman.type = PackageManager::kNone;
  addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());
  Json::Value ecu_data = Utils::parseJSONFile(temp_dir / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 2);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.provision.primary_ecu_serial);
  EXPECT_EQ(ecu_data["ecus"][1]["ecu_serial"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(ecu_data["ecus"][1]["hardware_identifier"].asString(), "secondary_hardware");
  EXPECT_EQ(ecu_data["ecus"][1]["clientKey"]["keytype"].asString(), "RSA");
  EXPECT_TRUE(ecu_data["ecus"][1]["clientKey"]["keyval"]["public"].asString().size() > 0);
}

bool success_InstallMultipleSecondaries = false;
int started_InstallMultipleSecondaries = 0;
int complete_InstallMultipleSecondaries = 0;
void process_events_InstallMultipleSecondaries(const std::shared_ptr<event::BaseEvent>& event) {
  if (event->variant == "InstallStarted") {
    ++started_InstallMultipleSecondaries;
  } else if (event->variant == "InstallComplete") {
    ++complete_InstallMultipleSecondaries;
  } else if (event->variant == "PutManifestComplete") {
    success_InstallMultipleSecondaries = true;
  }
}

/*
 * Verify successful installation of provided fake packages on multiple
 * secondaries. Skip fetching and downloading.
 */
TEST(Uptane, InstallMultipleSecondaries) {
  Config conf("tests/config/basic.toml");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  conf.provision.primary_ecu_serial = "testecuserial";
  conf.provision.primary_ecu_hardware_id = "testecuhwid";
  conf.uptane.director_server = http->tls_server + "/multisec/director";
  conf.uptane.repo_server = http->tls_server + "/multisec/repo";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_private_key_path = BasedPath("private.key");
  conf.storage.uptane_public_key_path = BasedPath("public.key");
  conf.pacman.sysroot = sysroot;
  conf.tls.server = http->tls_server;
  conf.uptane.running_mode = RunningMode::kFull;

  TemporaryDirectory temp_dir2;
  addDefaultSecondary(conf, temp_dir, "sec_serial1", "sec_hwid1");
  addDefaultSecondary(conf, temp_dir2, "sec_serial2", "sec_hwid2");

  std::shared_ptr<event::Channel> sig =
      std::make_shared<boost::signals2::signal<void(std::shared_ptr<event::BaseEvent>)>>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_InstallMultipleSecondaries;
  sig->connect(f_cb);

  auto storage = INvStorage::newStorage(conf.storage);
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, sig);
  EXPECT_NO_THROW(up->initialize());
  up->fetchMeta();

  size_t counter = 0;
  while (!success_InstallMultipleSecondaries) {
    sleep(1);
    ASSERT_LT(++counter, 20) << "Timed out waiting for secondary installation to complete.";
  }

  EXPECT_EQ(started_InstallMultipleSecondaries, 2);
  EXPECT_EQ(complete_InstallMultipleSecondaries, 2);
  Json::Value manifest = up->AssembleManifest();
  // Make sure filepath were correctly written and formatted.
  // installation_result has not been implemented for secondaries yet.
  EXPECT_EQ(manifest["sec_serial1"]["signed"]["installed_image"]["filepath"].asString(), "secondary_firmware.txt");
  EXPECT_EQ(manifest["sec_serial1"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 15);
  EXPECT_EQ(manifest["sec_serial2"]["signed"]["installed_image"]["filepath"].asString(), "secondary_firmware2.txt");
  EXPECT_EQ(manifest["sec_serial2"]["signed"]["installed_image"]["fileinfo"]["length"].asUInt(), 21);
  // Manifest should not have an installation result for the primary.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
}

/**
 * \verify{\tst{149}} Check that basic device info sent by aktualizr on provisioning are on server
 * Also test that installation works as expected with the fake package manager.
 * TODO: does that actually work? And is this the right way to test it anyway?
 */
TEST(Uptane, ProvisionOnServer) {
  TemporaryDirectory temp_dir;
  Config config("tests/config/basic.toml");
  const std::string server = "tst149";
  config.provision.server = server;
  config.tls.server = server;
  config.uptane.director_server = server + "/director";
  config.uptane.repo_server = server + "/repo";
  config.provision.device_id = "tst149_device_id";
  config.provision.primary_ecu_hardware_id = "tst149_hardware_identifier";
  config.provision.primary_ecu_serial = "tst149_ecu_serial";
  config.uptane.running_mode = RunningMode::kManual;
  config.storage.path = temp_dir.Path();

  auto storage = INvStorage::newStorage(config.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  std::vector<Uptane::Target> packages_to_install =
      makePackage(config.provision.primary_ecu_serial, config.provision.primary_ecu_hardware_id);

  auto up = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(up->initialize());
  EXPECT_NO_THROW(up->sendDeviceData());
  EXPECT_THROW(up->fetchMeta(), Uptane::InvalidMetadata);
  up->downloadImages(packages_to_install);
  up->uptaneInstall(packages_to_install);
}

TEST(Uptane, CheckOldProvision) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  int result = system((std::string("cp -rf tests/test_data/oldprovdir/* ") + temp_dir.PathString()).c_str());
  (void)result;
  Config config;
  config.tls.server = http->tls_server;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.storage.path = temp_dir.Path();

  // note: trigger migration from fs to sql
  auto storage = INvStorage::newStorage(config.storage);
  EXPECT_FALSE(storage->loadEcuRegistered());

  KeyManager keys(storage, config.keymanagerConfig());
  Initializer initializer(config.provision, storage, http, keys, {});

  EXPECT_TRUE(initializer.isSuccessful());
  EXPECT_TRUE(storage->loadEcuRegistered());
}

TEST(Uptane, fs_to_sql_full) {
  TemporaryDirectory temp_dir;
  int result = system((std::string("cp -rf tests/test_data/prov/* ") + temp_dir.PathString()).c_str());
  (void)result;
  StorageConfig config;
  config.type = StorageType::kSqlite;
  config.uptane_metadata_path = BasedPath("metadata");
  config.path = temp_dir.Path();

  config.uptane_private_key_path = BasedPath("ecukey.der");
  config.tls_cacert_path = BasedPath("root.crt");

  FSStorageRead fs_storage(config);

  std::string public_key;
  std::string private_key;
  fs_storage.loadPrimaryKeys(&public_key, &private_key);

  std::string ca;
  std::string cert;
  std::string pkey;
  fs_storage.loadTlsCreds(&ca, &cert, &pkey);

  std::string device_id;
  fs_storage.loadDeviceId(&device_id);

  EcuSerials serials;
  fs_storage.loadEcuSerials(&serials);

  bool ecu_registered = fs_storage.loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> installed_versions;
  fs_storage.loadInstalledVersions(&installed_versions);

  std::string director_root;
  std::string director_targets;
  std::string images_root;
  std::string images_targets;
  std::string images_timestamp;
  std::string images_snapshot;

  EXPECT_TRUE(fs_storage.loadLatestRoot(&director_root, Uptane::RepositoryType::Director));
  EXPECT_TRUE(fs_storage.loadNonRoot(&director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets()));
  EXPECT_TRUE(fs_storage.loadLatestRoot(&images_root, Uptane::RepositoryType::Images));
  EXPECT_TRUE(fs_storage.loadNonRoot(&images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot()));

  EXPECT_TRUE(boost::filesystem::exists(config.uptane_public_key_path.get(config.path)));
  EXPECT_TRUE(boost::filesystem::exists(config.uptane_private_key_path.get(config.path)));
  EXPECT_TRUE(boost::filesystem::exists(config.tls_cacert_path.get(config.path)));
  EXPECT_TRUE(boost::filesystem::exists(config.tls_clientcert_path.get(config.path)));
  EXPECT_TRUE(boost::filesystem::exists(config.tls_pkey_path.get(config.path)));

  boost::filesystem::path image_path = config.uptane_metadata_path.get(config.path) / "repo";
  boost::filesystem::path director_path = config.uptane_metadata_path.get(config.path) / "director";
  EXPECT_TRUE(boost::filesystem::exists(director_path / "1.root.json"));
  EXPECT_TRUE(boost::filesystem::exists(director_path / "targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(image_path / "1.root.json"));
  EXPECT_TRUE(boost::filesystem::exists(image_path / "targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(image_path / "timestamp.json"));
  EXPECT_TRUE(boost::filesystem::exists(image_path / "snapshot.json"));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "device_id")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "is_registered")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "primary_ecu_serial")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "primary_ecu_hardware_id")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "secondaries_list")));
  auto sql_storage = INvStorage::newStorage(config);

  EXPECT_FALSE(boost::filesystem::exists(config.uptane_public_key_path.get(config.path)));
  EXPECT_FALSE(boost::filesystem::exists(config.uptane_private_key_path.get(config.path)));
  EXPECT_FALSE(boost::filesystem::exists(config.tls_cacert_path.get(config.path)));
  EXPECT_FALSE(boost::filesystem::exists(config.tls_clientcert_path.get(config.path)));
  EXPECT_FALSE(boost::filesystem::exists(config.tls_pkey_path.get(config.path)));

  EXPECT_FALSE(boost::filesystem::exists(director_path / "root.json"));
  EXPECT_FALSE(boost::filesystem::exists(director_path / "targets.json"));
  EXPECT_FALSE(boost::filesystem::exists(director_path / "root.json"));
  EXPECT_FALSE(boost::filesystem::exists(director_path / "targets.json"));
  EXPECT_FALSE(boost::filesystem::exists(image_path / "timestamp.json"));
  EXPECT_FALSE(boost::filesystem::exists(image_path / "snapshot.json"));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, "device_id")));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, "is_registered")));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, "primary_ecu_serial")));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, "primary_ecu_hardware_id")));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, "secondaries_list")));

  std::string sql_public_key;
  std::string sql_private_key;
  sql_storage->loadPrimaryKeys(&sql_public_key, &sql_private_key);

  std::string sql_ca;
  std::string sql_cert;
  std::string sql_pkey;
  sql_storage->loadTlsCreds(&sql_ca, &sql_cert, &sql_pkey);

  std::string sql_device_id;
  sql_storage->loadDeviceId(&sql_device_id);

  EcuSerials sql_serials;
  sql_storage->loadEcuSerials(&sql_serials);

  bool sql_ecu_registered = sql_storage->loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> sql_installed_versions;
  sql_storage->loadInstalledVersions(&sql_installed_versions);

  std::string sql_director_root;
  std::string sql_director_targets;
  std::string sql_images_root;
  std::string sql_images_targets;
  std::string sql_images_timestamp;
  std::string sql_images_snapshot;

  sql_storage->loadLatestRoot(&sql_director_root, Uptane::RepositoryType::Director);
  sql_storage->loadNonRoot(&sql_director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets());
  sql_storage->loadLatestRoot(&sql_images_root, Uptane::RepositoryType::Images);
  sql_storage->loadNonRoot(&sql_images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets());
  sql_storage->loadNonRoot(&sql_images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp());
  sql_storage->loadNonRoot(&sql_images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot());

  EXPECT_EQ(sql_public_key, public_key);
  EXPECT_EQ(sql_private_key, private_key);
  EXPECT_EQ(sql_ca, ca);
  EXPECT_EQ(sql_cert, cert);
  EXPECT_EQ(sql_pkey, pkey);
  EXPECT_EQ(sql_device_id, device_id);
  EXPECT_EQ(sql_serials, serials);
  EXPECT_EQ(sql_ecu_registered, ecu_registered);
  EXPECT_EQ(sql_installed_versions, installed_versions);

  EXPECT_EQ(sql_director_root, director_root);
  EXPECT_EQ(sql_director_targets, director_targets);
  EXPECT_EQ(sql_images_root, images_root);
  EXPECT_EQ(sql_images_targets, images_targets);
  EXPECT_EQ(sql_images_timestamp, images_timestamp);
  EXPECT_EQ(sql_images_snapshot, images_snapshot);
}

TEST(Uptane, fs_to_sql_partial) {
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/prov/ecukey.der", temp_dir.Path() / "ecukey.der");
  boost::filesystem::copy_file("tests/test_data/prov/ecukey.pub", temp_dir.Path() / "ecukey.pub");

  StorageConfig config;
  config.type = StorageType::kSqlite;
  config.uptane_metadata_path = BasedPath("metadata");
  config.path = temp_dir.Path();

  config.uptane_private_key_path = BasedPath("ecukey.der");
  config.tls_cacert_path = BasedPath("root.crt");

  FSStorageRead fs_storage(config);

  std::string public_key;
  std::string private_key;
  fs_storage.loadPrimaryKeys(&public_key, &private_key);

  std::string ca;
  std::string cert;
  std::string pkey;
  fs_storage.loadTlsCreds(&ca, &cert, &pkey);

  std::string device_id;
  fs_storage.loadDeviceId(&device_id);

  EcuSerials serials;
  fs_storage.loadEcuSerials(&serials);

  bool ecu_registered = fs_storage.loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> installed_versions;
  fs_storage.loadInstalledVersions(&installed_versions);

  std::string director_root;
  std::string director_targets;
  std::string images_root;
  std::string images_targets;
  std::string images_timestamp;
  std::string images_snapshot;

  fs_storage.loadLatestRoot(&director_root, Uptane::RepositoryType::Director);
  fs_storage.loadNonRoot(&director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets());
  fs_storage.loadLatestRoot(&images_root, Uptane::RepositoryType::Images);
  fs_storage.loadNonRoot(&images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets());
  fs_storage.loadNonRoot(&images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp());
  fs_storage.loadNonRoot(&images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot());

  EXPECT_TRUE(boost::filesystem::exists(config.uptane_public_key_path.get(config.path)));
  EXPECT_TRUE(boost::filesystem::exists(config.uptane_private_key_path.get(config.path)));

  auto sql_storage = INvStorage::newStorage(config);

  EXPECT_FALSE(boost::filesystem::exists(config.uptane_public_key_path.get(config.path)));
  EXPECT_FALSE(boost::filesystem::exists(config.uptane_private_key_path.get(config.path)));

  std::string sql_public_key;
  std::string sql_private_key;
  sql_storage->loadPrimaryKeys(&sql_public_key, &sql_private_key);

  std::string sql_ca;
  std::string sql_cert;
  std::string sql_pkey;
  sql_storage->loadTlsCreds(&sql_ca, &sql_cert, &sql_pkey);

  std::string sql_device_id;
  sql_storage->loadDeviceId(&sql_device_id);

  EcuSerials sql_serials;
  sql_storage->loadEcuSerials(&sql_serials);

  bool sql_ecu_registered = sql_storage->loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> sql_installed_versions;
  sql_storage->loadInstalledVersions(&sql_installed_versions);

  std::string sql_director_root;
  std::string sql_director_targets;
  std::string sql_images_root;
  std::string sql_images_targets;
  std::string sql_images_timestamp;
  std::string sql_images_snapshot;

  sql_storage->loadLatestRoot(&sql_director_root, Uptane::RepositoryType::Director);
  sql_storage->loadNonRoot(&sql_director_targets, Uptane::RepositoryType::Director, Uptane::Role::Targets());
  sql_storage->loadLatestRoot(&sql_images_root, Uptane::RepositoryType::Images);
  sql_storage->loadNonRoot(&sql_images_targets, Uptane::RepositoryType::Images, Uptane::Role::Targets());
  sql_storage->loadNonRoot(&sql_images_timestamp, Uptane::RepositoryType::Images, Uptane::Role::Timestamp());
  sql_storage->loadNonRoot(&sql_images_snapshot, Uptane::RepositoryType::Images, Uptane::Role::Snapshot());

  EXPECT_EQ(sql_public_key, public_key);
  EXPECT_EQ(sql_private_key, private_key);
  EXPECT_EQ(sql_ca, ca);
  EXPECT_EQ(sql_cert, cert);
  EXPECT_EQ(sql_pkey, pkey);
  EXPECT_EQ(sql_device_id, device_id);
  EXPECT_EQ(sql_serials, serials);
  EXPECT_EQ(sql_ecu_registered, ecu_registered);
  EXPECT_EQ(sql_installed_versions, installed_versions);

  EXPECT_EQ(sql_director_root, director_root);
  EXPECT_EQ(sql_director_targets, director_targets);
  EXPECT_EQ(sql_images_root, images_root);
  EXPECT_EQ(sql_images_targets, images_targets);
  EXPECT_EQ(sql_images_timestamp, images_timestamp);
  EXPECT_EQ(sql_images_snapshot, images_snapshot);
}

TEST(Uptane, SaveVersion) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = BasedPath("ca.pem");
  config.storage.tls_clientcert_path = BasedPath("client.pem");
  config.storage.tls_pkey_path = BasedPath("pkey.pem");
  config.provision.device_id = "device_id";
  config.postUpdateValues();
  auto storage = INvStorage::newStorage(config.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 123;

  Uptane::Target t("target_name", target_json);
  storage->saveInstalledVersion(t);

  std::vector<Uptane::Target> installed_versions;
  storage->loadInstalledVersions(&installed_versions);

  auto f = std::find_if(installed_versions.begin(), installed_versions.end(),
                        [](const Uptane::Target& t_) { return t_.filename() == "target_name"; });
  EXPECT_NE(f, installed_versions.end());
  EXPECT_EQ(f->sha256Hash(), "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d");
  EXPECT_EQ(f->length(), 123);
}

TEST(Uptane, LoadVersion) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = BasedPath("ca.pem");
  config.storage.tls_clientcert_path = BasedPath("client.pem");
  config.storage.tls_pkey_path = BasedPath("pkey.pem");
  config.provision.device_id = "device_id";
  config.postUpdateValues();
  auto storage = INvStorage::newStorage(config.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 0;

  Uptane::Target t("target_name", target_json);
  storage->saveInstalledVersion(t);

  std::vector<Uptane::Target> versions;
  storage->loadInstalledVersions(&versions);
  EXPECT_EQ(t, versions[0]);
}

TEST(Uptane, krejectallTest) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.pacman.type = PackageManager::kNone;
  config.provision.device_id = "device_id";
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  Uptane::EcuSerial primary_ecu_serial("CA:FE:A6:D2:84:9D");
  Uptane::HardwareIdentifier primary_hw_id("primary_hw");
  Uptane::EcuSerial secondary_ecu_serial("secondary_ecu_serial");
  Uptane::HardwareIdentifier secondary_hw_id("secondary_hw");
  sota_client->hw_ids.insert(std::make_pair(primary_ecu_serial, primary_hw_id));
  sota_client->hw_ids.insert(std::make_pair(secondary_ecu_serial, secondary_hw_id));
  EXPECT_TRUE(sota_client->uptaneIteration());
}

TEST(Uptane, restoreVerify) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.uptane.director_server = http->tls_server + "/unstable/director";
  config.uptane.repo_server = http->tls_server + "/unstable/repo";
  config.pacman.type = PackageManager::kNone;
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  config.storage.uptane_metadata_path = BasedPath("metadata");
  config.storage.uptane_private_key_path = BasedPath("private.key");
  config.storage.uptane_public_key_path = BasedPath("public.key");
  addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);

  EXPECT_NO_THROW(sota_client->initialize());
  sota_client->AssembleManifest();
  // 1st attempt, don't get anything
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_FALSE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director));

  // 2nd attempt, get director root.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Director, Uptane::Role::Targets()));

  // 3rd attempt, get director targets.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director));
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Director, Uptane::Role::Targets()));
  EXPECT_FALSE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Images));

  // 4th attempt, get images root.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Images));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Images, Uptane::Role::Timestamp()));

  // 5th attempt, get images timestamp.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Images, Uptane::Role::Timestamp()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Images, Uptane::Role::Snapshot()));

  // 6th attempt, get images snapshot.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Images, Uptane::Role::Snapshot()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Images, Uptane::Role::Targets()));

  // 7th attempt, get images targets.json, successful iteration
  EXPECT_TRUE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Images, Uptane::Role::Targets()));
}

TEST(Uptane, offlineIteration) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.pacman.type = PackageManager::kNone;
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  config.storage.uptane_metadata_path = BasedPath("metadata");
  config.storage.uptane_private_key_path = BasedPath("private.key");
  config.storage.uptane_public_key_path = BasedPath("public.key");
  addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);

  EXPECT_NO_THROW(sota_client->initialize());
  sota_client->AssembleManifest();

  std::vector<Uptane::Target> targets_online;
  EXPECT_TRUE(sota_client->uptaneIteration());
  EXPECT_TRUE(sota_client->getNewTargets(&targets_online));

  std::vector<Uptane::Target> targets_offline;
  EXPECT_TRUE(sota_client->uptaneOfflineIteration(&targets_offline, nullptr));
  EXPECT_EQ(targets_online, targets_offline);
}

#ifdef BUILD_P11
TEST(Uptane, Pkcs11Provision) {
  Config config;
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir / "ca.pem");
  config.tls.cert_source = CryptoSource::kPkcs11;
  config.tls.pkey_source = CryptoSource::kPkcs11;
  config.p11.module = TEST_PKCS11_MODULE_PATH;
  config.p11.pass = "1234";
  config.p11.tls_clientcert_id = "01";
  config.p11.tls_pkey_id = "02";

  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = BasedPath("ca.pem");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  KeyManager keys(storage, config.keymanagerConfig());
  Initializer initializer(config.provision, storage, http, keys, {});

  EXPECT_TRUE(initializer.isSuccessful());
}
#endif

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
