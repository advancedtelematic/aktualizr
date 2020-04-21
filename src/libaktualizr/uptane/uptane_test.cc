#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include "json/json.h"

#include "crypto/p11engine.h"
#include "httpfake.h"
#include "primary/initializer.h"
#include "primary/sotauptaneclient.h"
#include "storage/fsstorage_read.h"
#include "storage/invstorage.h"
#include "test_utils.h"
#include "uptane/secondaryinterface.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"
#include "uptane_test_common.h"
#include "utilities/utils.h"

#ifdef BUILD_P11
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

static Config config_common() {
  Config config;
  config.uptane.key_type = KeyType::kED25519;
  return config;
}

TEST(Uptane, Verify) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  HttpResponse response = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit);
  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  Uptane::Root(Uptane::RepositoryType::Director(), response.getJson(), root);
}

/* Throw an exception if Root metadata is unsigned. */
TEST(Uptane, VerifyDataBad) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();
  data_json.removeMember("signatures");

  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), data_json, root), Uptane::UnmetThreshold);
}

/* Throw an exception if Root metadata has unknown signature types. */
TEST(Uptane, VerifyDataUnknownType) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();
  data_json["signatures"][0]["method"] = "badsignature";
  data_json["signatures"][1]["method"] = "badsignature";

  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), data_json, root), Uptane::SecurityException);
}

/* Throw an exception if Root metadata has invalid key IDs. */
TEST(Uptane, VerifyDataBadKeyId) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();

  data_json["signatures"][0]["keyid"] = "badkeyid";

  Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), data_json, root), Uptane::BadKeyId);
}

/* Throw an exception if Root metadata signature threshold is invalid. */
TEST(Uptane, VerifyDataBadThreshold) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  auto storage = INvStorage::newStorage(config.storage);
  Json::Value data_json = http->get(http->tls_server + "/director/root.json", HttpInterface::kNoLimit).getJson();
  data_json["signed"]["roles"]["root"]["threshold"] = -1;
  try {
    Uptane::Root root(Uptane::Root::Policy::kAcceptAll);
    Uptane::Root(Uptane::RepositoryType::Director(), data_json, root);
    FAIL() << "Illegal threshold should have thrown an error.";
  } catch (const Uptane::IllegalThreshold &ex) {
  } catch (const Uptane::UnmetThreshold &ex) {
  }
}

/* Get manifest from Primary.
 * Get manifest from Secondaries. */
TEST(Uptane, AssembleManifestGood) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.storage.path = temp_dir.Path();
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kSharedCred;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PACKAGE_MANAGER_NONE;
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  Json::Value manifest = sota_client->AssembleManifest()["ecu_version_manifests"];
  EXPECT_EQ(manifest.size(), 2);
  EXPECT_EQ(manifest["testecuserial"]["signed"]["ecu_serial"].asString(), config.provision.primary_ecu_serial);
  EXPECT_EQ(manifest["secondary_ecu_serial"]["signed"]["ecu_serial"].asString(), "secondary_ecu_serial");
  // Manifest should not have an installation result yet.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
  EXPECT_FALSE(manifest["secondary_ecu_serial"]["signed"].isMember("custom"));

  std::string counter_str = manifest["testecuserial"]["signed"]["report_counter"].asString();
  int64_t primary_ecu_report_counter = std::stoll(counter_str);
  Json::Value manifest2 = sota_client->AssembleManifest()["ecu_version_manifests"];
  std::string counter_str2 = manifest2["testecuserial"]["signed"]["report_counter"].asString();
  int64_t primary_ecu_report_counter2 = std::stoll(counter_str2);
  EXPECT_EQ(primary_ecu_report_counter2, primary_ecu_report_counter + 1);
}

/* Bad signatures are ignored when assembling the manifest. */
TEST(Uptane, AssembleManifestBad) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.storage.path = temp_dir.Path();
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kSharedCred;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PACKAGE_MANAGER_NONE;
  Primary::VirtualSecondaryConfig ecu_config =
      UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  /* Overwrite the Secondary's keys on disk. */
  std::string private_key, public_key;
  ASSERT_TRUE(Crypto::generateKeyPair(ecu_config.key_type, &public_key, &private_key));
  Utils::writeFile(ecu_config.full_client_dir / ecu_config.ecu_private_key, private_key);
  public_key = Utils::readFile("tests/test_data/public.key");
  Utils::writeFile(ecu_config.full_client_dir / ecu_config.ecu_public_key, public_key);

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  Json::Value manifest = sota_client->AssembleManifest()["ecu_version_manifests"];
  EXPECT_EQ(manifest.size(), 1);
  EXPECT_EQ(manifest["testecuserial"]["signed"]["ecu_serial"].asString(), config.provision.primary_ecu_serial);
  // Manifest should not have an installation result yet.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
  EXPECT_FALSE(manifest["secondary_ecu_serial"]["signed"].isMember("custom"));
}

/* Get manifest from Primary.
 * Get manifest from Secondaries.
 * Send manifest to the server. */
TEST(Uptane, PutManifest) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  config.storage.path = temp_dir.Path();
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kSharedCred;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PACKAGE_MANAGER_NONE;
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);

  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());
  EXPECT_TRUE(sota_client->putManifestSimple());

  Json::Value json = http->last_manifest;

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

class HttpPutManifestFail : public HttpFake {
 public:
  HttpPutManifestFail(const boost::filesystem::path &test_dir_in, std::string flavor = "")
      : HttpFake(test_dir_in, flavor) {}
  HttpResponse put(const std::string &url, const Json::Value &data) override {
    (void)data;
    return HttpResponse(url, 504, CURLE_OK, "");
  }
};

int num_events_PutManifestError = 0;
void process_events_PutManifestError(const std::shared_ptr<event::BaseEvent> &event) {
  std::cout << event->variant << "\n";
  if (event->variant == "PutManifestComplete") {
    EXPECT_FALSE(std::static_pointer_cast<event::PutManifestComplete>(event)->success);
    num_events_PutManifestError++;
  }
}

/*
 * Send PutManifestComplete event if send is unsuccessful
 */
TEST(Uptane, PutManifestError) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpPutManifestFail>(temp_dir.Path());

  Config conf("tests/config/basic.toml");
  conf.storage.path = temp_dir.Path();

  auto storage = INvStorage::newStorage(conf.storage);
  auto events_channel = std::make_shared<event::Channel>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_PutManifestError;
  events_channel->connect(f_cb);
  num_events_PutManifestError = 0;
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf, storage, http, events_channel);
  EXPECT_NO_THROW(sota_client->initialize());
  auto result = sota_client->putManifest();
  EXPECT_FALSE(result);
  EXPECT_EQ(num_events_PutManifestError, 1);
}

/*
 * Verify that failing to send the manifest will not prevent checking for
 * updates (which is the only way to recover if something goes wrong with
 * sending the manifest).
 */
TEST(Uptane, FetchMetaFail) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpPutManifestFail>(temp_dir.Path(), "noupdates");

  Config conf("tests/config/basic.toml");
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = http->tls_server;

  auto storage = INvStorage::newStorage(conf.storage);
  auto up = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf, storage, http);

  EXPECT_NO_THROW(up->initialize());
  result::UpdateCheck result = up->fetchMeta();
  EXPECT_EQ(result.status, result::UpdateStatus::kNoUpdatesAvailable);
}

unsigned int num_events_InstallTarget = 0;
unsigned int num_events_AllInstalls = 0;
void process_events_Install(const std::shared_ptr<event::BaseEvent> &event) {
  if (event->variant == "InstallTargetComplete") {
    auto concrete_event = std::static_pointer_cast<event::InstallTargetComplete>(event);
    if (num_events_InstallTarget <= 1) {
      EXPECT_TRUE(concrete_event->success);
    } else {
      EXPECT_FALSE(concrete_event->success);
    }
    num_events_InstallTarget++;
  }
  if (event->variant == "AllInstallsComplete") {
    auto concrete_event = std::static_pointer_cast<event::AllInstallsComplete>(event);
    if (num_events_AllInstalls == 0) {
      EXPECT_TRUE(concrete_event->result.dev_report.isSuccess());
    } else if (num_events_AllInstalls == 1) {
      EXPECT_FALSE(concrete_event->result.dev_report.isSuccess());
      EXPECT_EQ(concrete_event->result.dev_report.result_code, data::ResultCode::Numeric::kAlreadyProcessed);
    } else {
      EXPECT_FALSE(concrete_event->result.dev_report.isSuccess());
      EXPECT_EQ(concrete_event->result.dev_report.result_code, data::ResultCode::Numeric::kInternalError);
    }
    num_events_AllInstalls++;
  }
}

/*
 * Verify successful installation of a package.
 *
 * Identify ECU for each target.
 * Check if there are updates to install for the Primary.
 * Install a binary update on the Primary.
 * Store installation result for Primary.
 * Store installation result for device.
 * Check if an update is already installed.
 * Reject an update that matches the currently installed version's filename but
 * not the length and/or hashes.
 */
TEST(Uptane, InstallFakeGood) {
  Config conf("tests/config/basic.toml");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  conf.uptane.director_server = http->tls_server + "director";
  conf.uptane.repo_server = http->tls_server + "repo";
  conf.pacman.type = PACKAGE_MANAGER_NONE;
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = http->tls_server;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");
  conf.postUpdateValues();

  auto storage = INvStorage::newStorage(conf.storage);
  auto events_channel = std::make_shared<event::Channel>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_Install;
  events_channel->connect(f_cb);
  auto up = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf, storage, http, events_channel);
  EXPECT_NO_THROW(up->initialize());

  result::UpdateCheck update_result = up->fetchMeta();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  result::Download download_result = up->downloadImages(update_result.updates);
  EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);
  result::Install install_result1 = up->uptaneInstall(download_result.updates);
  EXPECT_TRUE(install_result1.dev_report.isSuccess());
  EXPECT_EQ(install_result1.dev_report.result_code, data::ResultCode::Numeric::kOk);

  // Make sure operation_result and filepath were correctly written and formatted.
  Json::Value manifest = up->AssembleManifest();
  EXPECT_EQ(manifest["ecu_version_manifests"]["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"]["filepath"].asString(),
            "primary_firmware.txt");
  EXPECT_EQ(
      manifest["ecu_version_manifests"]["secondary_ecu_serial"]["signed"]["installed_image"]["filepath"].asString(),
      "secondary_firmware.txt");

  EXPECT_EQ(num_events_InstallTarget, 2);
  EXPECT_EQ(num_events_AllInstalls, 1);
  Json::Value installation_report = manifest["installation_report"]["report"];
  EXPECT_EQ(installation_report["result"]["success"].asBool(), true);
  EXPECT_EQ(installation_report["result"]["code"].asString(), "OK");
  EXPECT_EQ(installation_report["items"][0]["ecu"].asString(), "CA:FE:A6:D2:84:9D");
  EXPECT_EQ(installation_report["items"][0]["result"]["success"].asBool(), true);
  EXPECT_EQ(installation_report["items"][0]["result"]["code"].asString(), "OK");
  EXPECT_EQ(installation_report["items"][1]["ecu"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(installation_report["items"][1]["result"]["success"].asBool(), true);
  EXPECT_EQ(installation_report["items"][1]["result"]["code"].asString(), "OK");

  // Second install to verify that we detect already installed updates
  // correctly.
  result::Install install_result2 = up->uptaneInstall(download_result.updates);
  EXPECT_FALSE(install_result2.dev_report.isSuccess());
  EXPECT_EQ(install_result2.dev_report.result_code, data::ResultCode::Numeric::kAlreadyProcessed);
  EXPECT_EQ(num_events_InstallTarget, 2);
  EXPECT_EQ(num_events_AllInstalls, 2);
  manifest = up->AssembleManifest();
  installation_report = manifest["installation_report"]["report"];
  EXPECT_EQ(installation_report["result"]["success"].asBool(), false);

  // Recheck updates in order to repopulate Director Targets metadata in the
  // database (it gets dropped after failure).
  result::UpdateCheck update_result2 = up->fetchMeta();
  EXPECT_EQ(update_result2.status, result::UpdateStatus::kNoUpdatesAvailable);

  // Remove the hashes from the current Target version stored in the database
  // for the Primary.
  boost::optional<Uptane::Target> current_version;
  EXPECT_TRUE(storage->loadInstalledVersions("CA:FE:A6:D2:84:9D", &current_version, nullptr));
  const auto bad_target = Uptane::Target(current_version->filename(), current_version->ecus(), std::vector<Hash>{},
                                         current_version->length());
  storage->saveInstalledVersion("CA:FE:A6:D2:84:9D", bad_target, InstalledVersionUpdateMode::kCurrent);

  // Third install to verify that we reject updates with the same filename but
  // different contents.
  result::Install install_result3 = up->uptaneInstall(download_result.updates);
  EXPECT_FALSE(install_result3.dev_report.isSuccess());
  EXPECT_EQ(install_result3.dev_report.result_code, data::ResultCode::Numeric::kInternalError);
  EXPECT_THROW(std::rethrow_exception(up->getLastException()), Uptane::TargetContentMismatch);
  EXPECT_EQ(num_events_InstallTarget, 2);
  EXPECT_EQ(num_events_AllInstalls, 3);
}

/*
 * Verify that installation will fail if the underlying data does not match the
 * target.
 */
TEST(Uptane, InstallFakeBad) {
  Config conf("tests/config/basic.toml");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  conf.uptane.director_server = http->tls_server + "director";
  conf.uptane.repo_server = http->tls_server + "repo";
  conf.pacman.type = PACKAGE_MANAGER_NONE;
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = http->tls_server;
  UptaneTestCommon::addDefaultSecondary(conf, temp_dir, "secondary_ecu_serial", "secondary_hw");
  conf.postUpdateValues();

  auto storage = INvStorage::newStorage(conf.storage);
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_Install;
  auto up = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf, storage, http);
  EXPECT_NO_THROW(up->initialize());

  result::UpdateCheck update_result = up->fetchMeta();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  result::Download download_result = up->downloadImages(update_result.updates);
  EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);

  std::string hash = download_result.updates[0].sha256Hash();
  std::transform(hash.begin(), hash.end(), hash.begin(), ::toupper);
  boost::filesystem::path image = temp_dir / "images" / hash;

  // Overwrite the file on disk with garbage so that the target verification
  // fails. First read the existing data so we can re-write it later.
  auto rhandle = storage->openTargetFile(download_result.updates[0]);
  const uint64_t length = download_result.updates[0].length();
  uint8_t content[length];
  EXPECT_EQ(rhandle->rread(content, length), length);
  rhandle->rclose();
  auto whandle = storage->allocateTargetFile(download_result.updates[0]);
  uint8_t content_bad[length + 1];
  memset(content_bad, 0, length + 1);
  EXPECT_EQ(whandle->wfeed(content_bad, 3), 3);
  whandle->wcommit();

  result::Install install_result = up->uptaneInstall(download_result.updates);
  EXPECT_FALSE(install_result.dev_report.isSuccess());
  EXPECT_EQ(install_result.dev_report.result_code, data::ResultCode::Numeric::kInternalError);

  // Try again with oversized data.
  whandle = storage->allocateTargetFile(download_result.updates[0]);
  EXPECT_EQ(whandle->wfeed(content_bad, length + 1), length + 1);
  whandle->wcommit();

  install_result = up->uptaneInstall(download_result.updates);
  EXPECT_FALSE(install_result.dev_report.isSuccess());
  EXPECT_EQ(install_result.dev_report.result_code, data::ResultCode::Numeric::kInternalError);

  // Try again with equally long data to make sure the hash check actually gets
  // triggered.
  whandle = storage->allocateTargetFile(download_result.updates[0]);
  EXPECT_EQ(whandle->wfeed(content_bad, length), length);
  whandle->wcommit();

  install_result = up->uptaneInstall(download_result.updates);
  EXPECT_FALSE(install_result.dev_report.isSuccess());
  EXPECT_EQ(install_result.dev_report.result_code, data::ResultCode::Numeric::kInternalError);

  // Try with the real data, but incomplete.
  whandle = storage->allocateTargetFile(download_result.updates[0]);
  EXPECT_EQ(whandle->wfeed(reinterpret_cast<uint8_t *>(content), length - 1), length - 1);
  whandle->wcommit();

  install_result = up->uptaneInstall(download_result.updates);
  EXPECT_FALSE(install_result.dev_report.isSuccess());
  EXPECT_EQ(install_result.dev_report.result_code, data::ResultCode::Numeric::kInternalError);

  // Restore the original data to the file so that verification succeeds.
  whandle = storage->allocateTargetFile(download_result.updates[0]);
  EXPECT_EQ(whandle->wfeed(reinterpret_cast<uint8_t *>(content), length), length);
  whandle->wcommit();

  install_result = up->uptaneInstall(download_result.updates);
  EXPECT_TRUE(install_result.dev_report.isSuccess());
  EXPECT_EQ(install_result.dev_report.result_code, data::ResultCode::Numeric::kOk);
}

bool EcuInstallationStartedReportGot = false;
class HttpFakeEvents : public HttpFake {
 public:
  HttpFakeEvents(const boost::filesystem::path &test_dir_in, std::string flavor = "")
      : HttpFake(test_dir_in, std::move(flavor)) {}

  virtual HttpResponse handle_event(const std::string &url, const Json::Value &data) override {
    for (const auto &event : data) {
      if (event["eventType"]["id"].asString() == "EcuInstallationStarted") {
        if (event["event"]["ecu"].asString() == "secondary_ecu_serial") {
          EcuInstallationStartedReportGot = true;
        }
      }
    }
    return HttpResponse(url, 200, CURLE_OK, "");
  }
};

class SecondaryInterfaceMock : public Uptane::SecondaryInterface {
 public:
  explicit SecondaryInterfaceMock(Primary::VirtualSecondaryConfig &sconfig_in) : sconfig(std::move(sconfig_in)) {
    std::string private_key, public_key;
    if (!Crypto::generateKeyPair(sconfig.key_type, &public_key, &private_key)) {
      throw std::runtime_error("Key generation failure");
    }
    public_key_ = PublicKey(public_key, sconfig.key_type);
    Json::Value manifest_unsigned;
    manifest_unsigned["key"] = "value";

    std::string b64sig = Utils::toBase64(
        Crypto::Sign(sconfig.key_type, nullptr, private_key, Utils::jsonToCanonicalStr(manifest_unsigned)));
    Json::Value signature;
    signature["method"] = "rsassa-pss";
    signature["sig"] = b64sig;
    signature["keyid"] = public_key_.KeyId();
    manifest_["signed"] = manifest_unsigned;
    manifest_["signatures"].append(signature);
  }
  std::string Type() const override { return "mock"; }
  PublicKey getPublicKey() const override { return public_key_; }

  Uptane::HardwareIdentifier getHwId() const override { return Uptane::HardwareIdentifier(sconfig.ecu_hardware_id); }
  Uptane::EcuSerial getSerial() const override {
    if (!sconfig.ecu_serial.empty()) {
      return Uptane::EcuSerial(sconfig.ecu_serial);
    }
    return Uptane::EcuSerial(public_key_.KeyId());
  }
  Uptane::Manifest getManifest() const override { return manifest_; }
  bool ping() const override { return true; }
  MOCK_METHOD(bool, putMetadataMock, (const Uptane::RawMetaPack &));
  MOCK_METHOD(int32_t, getRootVersionMock, (bool), (const));

  bool putMetadata(const Uptane::RawMetaPack &meta_pack) override {
    putMetadataMock(meta_pack);
    return true;
  }
  int32_t getRootVersion(bool director) const override { return getRootVersionMock(director); }

  bool putRoot(const std::string &, bool) override { return true; }
  virtual data::ResultCode::Numeric sendFirmware(const Uptane::Target &) override {
    return data::ResultCode::Numeric::kOk;
  }
  virtual data::ResultCode::Numeric install(const Uptane::Target &) override { return data::ResultCode::Numeric::kOk; }

  PublicKey public_key_;
  Json::Value manifest_;

  Primary::VirtualSecondaryConfig sconfig;
};

MATCHER_P(matchMeta, meta, "") {
  return (arg.director_root == meta.director_root) && (arg.image_root == meta.image_root) &&
         (arg.director_targets == meta.director_targets) && (arg.image_timestamp == meta.image_timestamp) &&
         (arg.image_snapshot == meta.image_snapshot) && (arg.image_targets == meta.image_targets);
}

/*
 * Send metadata to Secondary ECUs
 * Send EcuInstallationStartedReport to server for Secondaries
 */
TEST(Uptane, SendMetadataToSecondary) {
  Config conf("tests/config/basic.toml");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeEvents>(temp_dir.Path(), "hasupdates");
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = http->tls_server;

  Primary::VirtualSecondaryConfig ecu_config;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path();
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hw";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir / "firmware.txt";
  ecu_config.target_name_path = temp_dir / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir / "secondary_metadata";

  auto sec = std::make_shared<SecondaryInterfaceMock>(ecu_config);
  auto storage = INvStorage::newStorage(conf.storage);
  auto up = std_::make_unique<UptaneTestCommon::TestUptaneClient>(conf, storage, http);
  up->addSecondary(sec);
  EXPECT_NO_THROW(up->initialize());
  result::UpdateCheck update_result = up->fetchMeta();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);

  Uptane::RawMetaPack meta;
  storage->loadLatestRoot(&meta.director_root, Uptane::RepositoryType::Director());
  storage->loadNonRoot(&meta.director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
  storage->loadLatestRoot(&meta.image_root, Uptane::RepositoryType::Image());
  storage->loadNonRoot(&meta.image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
  storage->loadNonRoot(&meta.image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());
  storage->loadNonRoot(&meta.image_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());

  EXPECT_CALL(*sec, putMetadataMock(matchMeta(meta)));
  result::Download download_result = up->downloadImages(update_result.updates);
  EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);
  result::Install install_result = up->uptaneInstall(download_result.updates);
  EXPECT_TRUE(install_result.dev_report.isSuccess());
  EXPECT_EQ(install_result.dev_report.result_code, data::ResultCode::Numeric::kOk);
  EXPECT_TRUE(EcuInstallationStartedReportGot);
}

/* Register Secondary ECUs with Director. */
TEST(Uptane, UptaneSecondaryAdd) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config = config_common();
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kSharedCred;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.tls.server = http->tls_server;
  config.provision.primary_ecu_serial = "testecuserial";
  config.storage.path = temp_dir.Path();
  config.pacman.type = PACKAGE_MANAGER_NONE;
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  /* Verify the correctness of the metadata sent to the server about the
   * Secondary. */
  Json::Value ecu_data = Utils::parseJSONFile(temp_dir / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 2);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.provision.primary_ecu_serial);
  EXPECT_EQ(ecu_data["ecus"][1]["ecu_serial"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(ecu_data["ecus"][1]["hardware_identifier"].asString(), "secondary_hardware");
  EXPECT_EQ(ecu_data["ecus"][1]["clientKey"]["keytype"].asString(), "RSA");
  EXPECT_TRUE(ecu_data["ecus"][1]["clientKey"]["keyval"]["public"].asString().size() > 0);
}

/* Adding multiple Secondaries with the same serial throws an error */
TEST(Uptane, UptaneSecondaryAddSameSerial) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  Config config = config_common();
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kSharedCred;
  config.pacman.type = PACKAGE_MANAGER_NONE;
  config.storage.path = temp_dir.Path();

  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware_new");
  ImageReaderProvider image_reader = std::bind(&INvStorage::openTargetFile, storage.get(), std::placeholders::_1);
  EXPECT_THROW(
      sota_client->addSecondary(std::make_shared<Primary::VirtualSecondary>(
          Primary::VirtualSecondaryConfig::create_from_file(config.uptane.secondary_config_file)[0], image_reader)),
      std::runtime_error);
}

#if 0
// TODO: this test needs some refresh, it relies on the behaviour of
// UptaneTestCommon::TestUptaneClient which differs from actual SotaUptaneClient
/*
 * Identify previously unknown Secondaries
 * Identify currently unavailable Secondaries
 */
TEST(Uptane, UptaneSecondaryMisconfigured) {
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  {
    Config config = config_common();
    config.provision.provision_path = temp_dir / "cred.zip";
    config.provision.mode = ProvisionMode::kSharedCred;
    config.pacman.type = PACKAGE_MANAGER_NONE;
    config.storage.path = temp_dir.Path();
    UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

    auto storage = INvStorage::newStorage(config.storage);
    auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
    EXPECT_NO_THROW(sota_client->initialize());

    std::vector<MisconfiguredEcu> ecus;
    storage->loadMisconfiguredEcus(&ecus);
    EXPECT_EQ(ecus.size(), 0);
  }
  {
    Config config = config_common();
    config.provision.provision_path = temp_dir / "cred.zip";
    config.provision.mode = ProvisionMode::kSharedCred;
    config.pacman.type = PACKAGE_MANAGER_NONE;
    config.storage.path = temp_dir.Path();
    auto storage = INvStorage::newStorage(config.storage);
    UptaneTestCommon::addDefaultSecondary(config, temp_dir, "new_secondary_ecu_serial", "new_secondary_hardware");
    auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
    EXPECT_NO_THROW(sota_client->initialize());

    std::vector<MisconfiguredEcu> ecus;
    storage->loadMisconfiguredEcus(&ecus);
    EXPECT_EQ(ecus.size(), 2);
    if (ecus[0].serial.ToString() == "new_secondary_ecu_serial") {
      EXPECT_EQ(ecus[0].state, EcuState::kNotRegistered);
      EXPECT_EQ(ecus[1].serial.ToString(), "secondary_ecu_serial");
      EXPECT_EQ(ecus[1].state, EcuState::kOld);
    } else if (ecus[0].serial.ToString() == "secondary_ecu_serial") {
      EXPECT_EQ(ecus[0].state, EcuState::kOld);
      EXPECT_EQ(ecus[1].serial.ToString(), "new_secondary_ecu_serial");
      EXPECT_EQ(ecus[1].state, EcuState::kNotRegistered);
    } else {
      FAIL() << "Unexpected Secondary serial in storage: " << ecus[0].serial.ToString();
    }
  }
  {
    Config config = config_common();
    config.provision.provision_path = temp_dir / "cred.zip";
    config.provision.mode = ProvisionMode::kSharedCred;
    config.pacman.type = PACKAGE_MANAGER_NONE;
    config.storage.path = temp_dir.Path();
    auto storage = INvStorage::newStorage(config.storage);
    UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");
    auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
    EXPECT_NO_THROW(sota_client->initialize());

    std::vector<MisconfiguredEcu> ecus;
    storage->loadMisconfiguredEcus(&ecus);
    EXPECT_EQ(ecus.size(), 0);
  }
}
#endif

/**
 * Check that basic device info sent by aktualizr during provisioning matches
 * our expectations.
 *
 * Ideally, we would compare what we have with what the server reports, but in
 * lieu of that, we can check what we send via http.
 */
class HttpFakeProv : public HttpFake {
 public:
  HttpFakeProv(const boost::filesystem::path &test_dir_in, std::string flavor = "")
      : HttpFake(test_dir_in, std::move(flavor)) {}

  HttpResponse post(const std::string &url, const Json::Value &data) override {
    std::cout << "post " << url << "\n";

    if (url.find("/devices") != std::string::npos) {
      devices_count++;
      EXPECT_EQ(data["deviceId"].asString(), "tst149_device_id");
      return HttpResponse(Utils::readFile("tests/test_data/cred.p12"), 200, CURLE_OK, "");
    } else if (url.find("/director/ecus") != std::string::npos) {
      /* Register Primary ECU with Director. */
      ecus_count++;
      EXPECT_EQ(data["primary_ecu_serial"].asString(), "CA:FE:A6:D2:84:9D");
      EXPECT_EQ(data["ecus"][0]["hardware_identifier"].asString(), "primary_hw");
      EXPECT_EQ(data["ecus"][0]["ecu_serial"].asString(), "CA:FE:A6:D2:84:9D");
      if (ecus_count == 1) {
        return HttpResponse("{}", 200, CURLE_OK, "");
      } else {
        return HttpResponse(R"({"code":"ecu_already_registered"})", 409, CURLE_OK, "");
      }
    } else if (url.find("/events") != std::string::npos) {
      return handle_event(url, data);
    }
    EXPECT_EQ(0, 1) << "Unexpected post to URL: " << url;
    return HttpResponse("", 400, CURLE_OK, "");
  }

  HttpResponse handle_event_single(const Json::Value &event) {
    if (event["eventType"]["id"] == "DownloadProgressReport") {
      return HttpResponse("", 200, CURLE_OK, "");
    }
    const std::string event_type = event["eventType"]["id"].asString();
    const std::string serial = event["event"]["ecu"].asString();
    std::cout << "Got " << event_type << " event\n";
    ++events_seen;
    switch (events_seen) {
      case 0:
        EXPECT_EQ(event_type, "SendDeviceDataComplete");
        break;
      case 1:
      case 2:
      case 3:
      case 4:
        if (event_type == "EcuDownloadStarted") {
          if (serial == "CA:FE:A6:D2:84:9D") {
            ++primary_download_start;
          } else if (serial == "secondary_ecu_serial") {
            ++secondary_download_start;
          }
        } else if (event_type == "EcuDownloadCompleted") {
          if (serial == "CA:FE:A6:D2:84:9D") {
            ++primary_download_complete;
          } else if (serial == "secondary_ecu_serial") {
            ++secondary_download_complete;
          }
        }
        if (events_seen == 4) {
          EXPECT_EQ(primary_download_start, 1);
          EXPECT_EQ(primary_download_complete, 1);
          EXPECT_EQ(secondary_download_start, 1);
          EXPECT_EQ(secondary_download_complete, 1);
        }
        break;
      case 5:
        /* Send EcuInstallationStartedReport to server for Primary. */
        EXPECT_EQ(event_type, "EcuInstallationStarted");
        EXPECT_EQ(serial, "CA:FE:A6:D2:84:9D");
        break;
      case 6:
        /* Send EcuInstallationCompletedReport to server for Primary. */
        EXPECT_EQ(event_type, "EcuInstallationCompleted");
        EXPECT_EQ(serial, "CA:FE:A6:D2:84:9D");
        break;
      case 7:
        /* Send EcuInstallationStartedReport to server for secondaries. */
        EXPECT_EQ(event_type, "EcuInstallationStarted");
        EXPECT_EQ(serial, "secondary_ecu_serial");
        break;
      case 8:
        /* Send EcuInstallationCompletedReport to server for secondaries. */
        EXPECT_EQ(event_type, "EcuInstallationCompleted");
        EXPECT_EQ(serial, "secondary_ecu_serial");
        break;
      default:
        std::cout << "Unexpected event: " << event_type;
        EXPECT_EQ(0, 1);
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  HttpResponse handle_event(const std::string &url, const Json::Value &data) override {
    (void)url;
    for (const Json::Value &ev : data) {
      handle_event_single(ev);
    }
    return HttpResponse("", 200, CURLE_OK, "");
  }

  HttpResponse put(const std::string &url, const Json::Value &data) override {
    std::cout << "put " << url << "\n";
    if (url.find("core/installed") != std::string::npos) {
      /* Send a list of installed packages to the server. */
      installed_count++;
      EXPECT_EQ(data.size(), 1);
      EXPECT_EQ(data[0]["name"].asString(), "fake-package");
      EXPECT_EQ(data[0]["version"].asString(), "1.0");
    } else if (url.find("/director/manifest") != std::string::npos) {
      /* Get manifest from Primary.
       * Get Primary installation result.
       * Send manifest to the server. */
      manifest_count++;
      std::string file_primary;
      std::string file_secondary;
      std::string hash_primary;
      std::string hash_secondary;
      if (manifest_count <= 2) {
        file_primary = "unknown";
        file_secondary = "noimage";
        // Check for default initial value of packagemanagerfake.
        hash_primary = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
        hash_secondary = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
      } else {
        file_primary = "primary_firmware.txt";
        file_secondary = "secondary_firmware.txt";
        const Json::Value json = Utils::parseJSON(Utils::readFile(meta_dir / "director/targets_hasupdates.json"));
        const Json::Value targets_list = json["signed"]["targets"];
        hash_primary = targets_list["primary_firmware.txt"]["hashes"]["sha256"].asString();
        hash_secondary = targets_list["secondary_firmware.txt"]["hashes"]["sha256"].asString();
      }
      const Json::Value manifest = data["signed"]["ecu_version_manifests"];
      const Json::Value manifest_primary = manifest["CA:FE:A6:D2:84:9D"]["signed"]["installed_image"];
      const Json::Value manifest_secondary = manifest["secondary_ecu_serial"]["signed"]["installed_image"];
      EXPECT_EQ(file_primary, manifest_primary["filepath"].asString());
      EXPECT_EQ(file_secondary, manifest_secondary["filepath"].asString());
      EXPECT_EQ(manifest_primary["fileinfo"]["hashes"]["sha256"].asString(), hash_primary);
      EXPECT_EQ(manifest_secondary["fileinfo"]["hashes"]["sha256"].asString(), hash_secondary);
    } else if (url.find("/system_info/network") != std::string::npos) {
      /* Send networking info to the server. */
      network_count++;
      Json::Value nwinfo = Utils::getNetworkInfo();
      EXPECT_EQ(nwinfo["local_ipv4"].asString(), data["local_ipv4"].asString());
      EXPECT_EQ(nwinfo["mac"].asString(), data["mac"].asString());
      EXPECT_EQ(nwinfo["hostname"].asString(), data["hostname"].asString());
    } else if (url.find("/system_info") != std::string::npos) {
      /* Send hardware info to the server. */
      system_info_count++;
      Json::Value hwinfo = Utils::getHardwareInfo();
      EXPECT_EQ(hwinfo["id"].asString(), data["id"].asString());
      EXPECT_EQ(hwinfo["description"].asString(), data["description"].asString());
      EXPECT_EQ(hwinfo["class"].asString(), data["class"].asString());
      EXPECT_EQ(hwinfo["product"].asString(), data["product"].asString());
    } else {
      EXPECT_EQ(0, 1) << "Unexpected put to URL: " << url;
    }

    return HttpFake::put(url, data);
  }

  size_t events_seen{0};
  int devices_count{0};
  int ecus_count{0};
  int manifest_count{0};
  int installed_count{0};
  int system_info_count{0};
  int network_count{0};

 private:
  int primary_download_start{0};
  int primary_download_complete{0};
  int secondary_download_start{0};
  int secondary_download_complete{0};
};

/* Provision with a fake server and check for the exact number of expected
 * calls to each endpoint.
 * Use a provided hardware ID
 * Send SendDeviceDataComplete event
 */
TEST(Uptane, ProvisionOnServer) {
  RecordProperty("zephyr_key", "OTA-984,TST-149");
  TemporaryDirectory temp_dir;
  Config config("tests/config/basic.toml");
  auto http = std::make_shared<HttpFakeProv>(temp_dir.Path(), "hasupdates");
  const std::string &server = http->tls_server;
  config.provision.server = server;
  config.tls.server = server;
  config.uptane.director_server = server + "/director";
  config.uptane.repo_server = server + "/repo";
  config.provision.ecu_registration_endpoint = server + "/director/ecus";
  config.provision.device_id = "tst149_device_id";
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  config.storage.path = temp_dir.Path();
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");

  auto storage = INvStorage::newStorage(config.storage);
  auto events_channel = std::make_shared<event::Channel>();
  auto up = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http, events_channel);

  EXPECT_EQ(http->devices_count, 0);
  EXPECT_EQ(http->ecus_count, 0);
  EXPECT_EQ(http->manifest_count, 0);
  EXPECT_EQ(http->installed_count, 0);
  EXPECT_EQ(http->system_info_count, 0);
  EXPECT_EQ(http->network_count, 0);

  EXPECT_NO_THROW(up->initialize());
  EcuSerials serials;
  storage->loadEcuSerials(&serials);
  EXPECT_EQ(serials[0].second.ToString(), "primary_hw");

  EXPECT_EQ(http->devices_count, 1);
  EXPECT_EQ(http->ecus_count, 1);

  EXPECT_NO_THROW(up->sendDeviceData());
  EXPECT_EQ(http->manifest_count, 1);
  EXPECT_EQ(http->installed_count, 1);
  EXPECT_EQ(http->system_info_count, 1);
  EXPECT_EQ(http->network_count, 1);

  result::UpdateCheck update_result = up->fetchMeta();
  EXPECT_EQ(update_result.status, result::UpdateStatus::kUpdatesAvailable);
  EXPECT_EQ(http->manifest_count, 2);

  // Test installation to make sure the metadata put to the server is correct.
  result::Download download_result = up->downloadImages(update_result.updates);
  EXPECT_EQ(download_result.status, result::DownloadStatus::kSuccess);
  result::Install install_result = up->uptaneInstall(download_result.updates);
  EXPECT_TRUE(install_result.dev_report.isSuccess());
  EXPECT_EQ(install_result.dev_report.result_code, data::ResultCode::Numeric::kOk);
  up->putManifest();

  EXPECT_EQ(http->devices_count, 1);
  EXPECT_EQ(http->ecus_count, 1);
  EXPECT_EQ(http->manifest_count, 3);
  EXPECT_EQ(http->installed_count, 1);
  EXPECT_EQ(http->system_info_count, 1);
  EXPECT_EQ(http->network_count, 1);
  EXPECT_EQ(http->events_seen, 8);
}

/* Migrate from the legacy filesystem storage. */
TEST(Uptane, FsToSqlFull) {
  TemporaryDirectory temp_dir;
  Utils::copyDir("tests/test_data/prov", temp_dir.Path());
  ASSERT_GE(chmod(temp_dir.Path().c_str(), S_IRWXU), 0);
  StorageConfig config;
  config.type = StorageType::kSqlite;
  config.path = temp_dir.Path();

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

  bool ecu_registered = fs_storage.loadEcuRegistered();

  std::vector<Uptane::Target> fs_installed_versions;
  std::vector<Uptane::Target> fixed_installed_versions;
  fs_storage.loadInstalledVersions(&fs_installed_versions, nullptr);
  // Add the serial/hwid mapping to match what the SQL storage will do when
  // reading it back after it has been copied from FS storage.
  for (auto &target : fs_installed_versions) {
    Json::Value dump = target.toDebugJson();
    dump["custom"]["ecuIdentifiers"][serials[0].first.ToString()]["hardwareId"] = serials[0].second.ToString();
    fixed_installed_versions.emplace_back(Uptane::Target(target.filename(), dump));
  }

  std::string director_root;
  std::string director_targets;
  std::string image_root;
  std::string image_targets;
  std::string image_timestamp;
  std::string image_snapshot;

  EXPECT_TRUE(fs_storage.loadLatestRoot(&director_root, Uptane::RepositoryType::Director()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_TRUE(fs_storage.loadLatestRoot(&image_root, Uptane::RepositoryType::Image()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&image_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));

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

  bool sql_ecu_registered = sql_storage->loadEcuRegistered();

  std::vector<Uptane::Target> sql_installed_versions;
  sql_storage->loadPrimaryInstallationLog(&sql_installed_versions, true);

  std::string sql_director_root;
  std::string sql_director_targets;
  std::string sql_image_root;
  std::string sql_image_targets;
  std::string sql_image_timestamp;
  std::string sql_image_snapshot;

  sql_storage->loadLatestRoot(&sql_director_root, Uptane::RepositoryType::Director());
  sql_storage->loadNonRoot(&sql_director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
  sql_storage->loadLatestRoot(&sql_image_root, Uptane::RepositoryType::Image());
  sql_storage->loadNonRoot(&sql_image_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());
  sql_storage->loadNonRoot(&sql_image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
  sql_storage->loadNonRoot(&sql_image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());

  EXPECT_EQ(sql_public_key, public_key);
  EXPECT_EQ(sql_private_key, private_key);
  EXPECT_EQ(sql_ca, ca);
  EXPECT_EQ(sql_cert, cert);
  EXPECT_EQ(sql_pkey, pkey);
  EXPECT_EQ(sql_device_id, device_id);
  EXPECT_EQ(sql_serials, serials);
  EXPECT_EQ(sql_ecu_registered, ecu_registered);
  EXPECT_TRUE(Uptane::MatchTargetVector(sql_installed_versions, fixed_installed_versions));

  EXPECT_EQ(sql_director_root, director_root);
  EXPECT_EQ(sql_director_targets, director_targets);
  EXPECT_EQ(sql_image_root, image_root);
  EXPECT_EQ(sql_image_targets, image_targets);
  EXPECT_EQ(sql_image_timestamp, image_timestamp);
  EXPECT_EQ(sql_image_snapshot, image_snapshot);
}

/* Import a list of installed packages into the storage. */
TEST(Uptane, InstalledVersionImport) {
  Config config = config_common();

  TemporaryDirectory temp_dir;
  Utils::createDirectories(temp_dir / "import", S_IRWXU);
  config.storage.path = temp_dir.Path();
  config.import.base_path = temp_dir / "import";
  config.postUpdateValues();

  boost::filesystem::copy_file("tests/test_data/prov/installed_versions",
                               temp_dir.Path() / "import/installed_versions");

  // test basic import
  auto storage = INvStorage::newStorage(config.storage);
  storage->importData(config.import);

  boost::optional<Uptane::Target> current_version;
  storage->loadPrimaryInstalledVersions(&current_version, nullptr);
  EXPECT_TRUE(!!current_version);
  EXPECT_EQ(current_version->filename(), "master-863de625f305413dc3be306afab7c3f39d8713045cfff812b3af83f9722851f0");

  // check that data is not re-imported later: store new data, reload a new
  // storage with import and see that the new data is still there
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 123;
  Uptane::Target new_installed_version{"filename", target_json};
  storage->savePrimaryInstalledVersion(new_installed_version, InstalledVersionUpdateMode::kCurrent);

  auto new_storage = INvStorage::newStorage(config.storage);
  new_storage->importData(config.import);

  current_version = boost::none;
  new_storage->loadPrimaryInstalledVersions(&current_version, nullptr);
  EXPECT_TRUE(!!current_version);
  EXPECT_EQ(current_version->filename(), "filename");
}

/* Store a list of installed package versions. */
TEST(Uptane, SaveAndLoadVersion) {
  TemporaryDirectory temp_dir;
  Config config = config_common();
  config.storage.path = temp_dir.Path();
  config.provision.device_id = "device_id";
  config.postUpdateValues();
  auto storage = INvStorage::newStorage(config.storage);

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 123;

  Uptane::Target t("target_name", target_json);
  storage->savePrimaryInstalledVersion(t, InstalledVersionUpdateMode::kCurrent);

  boost::optional<Uptane::Target> current_version;
  storage->loadPrimaryInstalledVersions(&current_version, nullptr);

  EXPECT_TRUE(!!current_version);
  EXPECT_EQ(current_version->sha256Hash(), "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d");
  EXPECT_EQ(current_version->length(), 123);
  EXPECT_TRUE(current_version->MatchTarget(t));
}

class HttpFakeUnstable : public HttpFake {
 public:
  HttpFakeUnstable(const boost::filesystem::path &test_dir_in) : HttpFake(test_dir_in, "hasupdates") {}
  HttpResponse get(const std::string &url, int64_t maxsize) override {
    if (unstable_valid_count >= unstable_valid_num) {
      return HttpResponse({}, 503, CURLE_OK, "");
    } else {
      ++unstable_valid_count;
      return HttpFake::get(url, maxsize);
    }
  }

  void setUnstableValidNum(int num) {
    unstable_valid_num = num;
    unstable_valid_count = 0;
  }

  int unstable_valid_num{0};
  int unstable_valid_count{0};
};

/* Recover from an interrupted Uptane iteration.
 * Fetch metadata from the Director.
 * Check metadata from the Director.
 * Identify targets for known ECUs.
 * Fetch metadata from the Image repo.
 * Check metadata from the Image repo.
 *
 * This is a bit fragile because it depends upon a precise number of HTTP get
 * requests being made. If that changes, this test will need to be adjusted. */
TEST(Uptane, restoreVerify) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeUnstable>(temp_dir.Path());
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.pacman.type = PACKAGE_MANAGER_NONE;
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);

  EXPECT_NO_THROW(sota_client->initialize());
  sota_client->AssembleManifest();
  // 1st attempt, don't get anything
  EXPECT_THROW(sota_client->uptaneIteration(nullptr, nullptr), Uptane::MetadataFetchFailure);
  EXPECT_FALSE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director()));

  // 2nd attempt, get Director root.json
  http->setUnstableValidNum(1);
  EXPECT_THROW(sota_client->uptaneIteration(nullptr, nullptr), Uptane::MetadataFetchFailure);
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));

  // 3rd attempt, get Director targets.json
  http->setUnstableValidNum(2);
  EXPECT_THROW(sota_client->uptaneIteration(nullptr, nullptr), Uptane::MetadataFetchFailure);
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director()));
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_FALSE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Image()));

  // 4th attempt, get Image repo root.json
  http->setUnstableValidNum(3);
  EXPECT_THROW(sota_client->uptaneIteration(nullptr, nullptr), Uptane::MetadataFetchFailure);
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Image()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));

  // 5th attempt, get Image repo timestamp.json
  http->setUnstableValidNum(4);
  EXPECT_THROW(sota_client->uptaneIteration(nullptr, nullptr), Uptane::MetadataFetchFailure);
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));

  // 6th attempt, get Image repo snapshot.json
  http->setUnstableValidNum(5);
  EXPECT_THROW(sota_client->uptaneIteration(nullptr, nullptr), Uptane::MetadataFetchFailure);
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));

  // 7th attempt, get Image repo targets.json, successful iteration
  http->setUnstableValidNum(6);
  EXPECT_NO_THROW(sota_client->uptaneIteration(nullptr, nullptr));
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));
}

/* Fetch metadata from the Director.
 * Check metadata from the Director.
 * Identify targets for known ECUs.
 * Fetch metadata from the Image repo.
 * Check metadata from the Image repo. */
TEST(Uptane, offlineIteration) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.pacman.type = PACKAGE_MANAGER_NONE;
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  std::vector<Uptane::Target> targets_online;
  EXPECT_NO_THROW(sota_client->uptaneIteration(&targets_online, nullptr));

  std::vector<Uptane::Target> targets_offline;
  EXPECT_NO_THROW(sota_client->uptaneOfflineIteration(&targets_offline, nullptr));
  EXPECT_TRUE(Uptane::MatchTargetVector(targets_online, targets_offline));
}

/*
 * Ignore updates for unrecognized ECUs.
 * Reject targets which do not match a known ECU.
 */
TEST(Uptane, IgnoreUnknownUpdate) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.pacman.type = PACKAGE_MANAGER_NONE;
  config.provision.primary_ecu_serial = "primary_ecu";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = std_::make_unique<UptaneTestCommon::TestUptaneClient>(config, storage, http);

  EXPECT_NO_THROW(sota_client->initialize());

  auto result = sota_client->fetchMeta();
  EXPECT_EQ(result.status, result::UpdateStatus::kError);
  std::vector<Uptane::Target> packages_to_install = UptaneTestCommon::makePackage("testecuserial", "testecuhwid");
  auto report = sota_client->uptaneInstall(packages_to_install);
  EXPECT_EQ(report.ecu_reports.size(), 0);
}

#ifdef BUILD_P11
TEST(Uptane, Pkcs11Provision) {
  Config config;
  TemporaryDirectory temp_dir;
  Utils::createDirectories(temp_dir / "import", S_IRWXU);
  boost::filesystem::copy_file("tests/test_data/device_cred_prov/ca.pem", temp_dir / "import/root.crt");
  config.tls.cert_source = CryptoSource::kPkcs11;
  config.tls.pkey_source = CryptoSource::kPkcs11;
  config.p11.module = TEST_PKCS11_MODULE_PATH;
  config.p11.pass = "1234";
  config.p11.tls_clientcert_id = "01";
  config.p11.tls_pkey_id = "02";
  config.import.base_path = (temp_dir / "import").string();
  config.import.tls_cacert_path = BasedPath("root.crt");

  config.storage.path = temp_dir.Path();
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  storage->importData(config.import);
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  KeyManager keys(storage, config.keymanagerConfig());

  EXPECT_NO_THROW(Initializer(config.provision, storage, http, keys, {}));
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  logger_init();
  logger_set_threshold(boost::log::trivial::trace);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
