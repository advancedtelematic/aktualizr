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
  Uptane::Root(Uptane::RepositoryType::Director(), response.getJson(), root);
}

/* Throw an exception if a TUF root is unsigned. */
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
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), data_json, root), Uptane::UnmetThreshold);
}

/* Throw an exception if a TUF root has unknown signature types. */
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
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), data_json, root), Uptane::SecurityException);
}

/* Throw an exception if a TUF root has invalid key IDs. */
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
  EXPECT_THROW(Uptane::Root(Uptane::RepositoryType::Director(), data_json, root), Uptane::BadKeyId);
}

/* Throw an exception if a TUF root signature threshold is invalid. */
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
    Uptane::Root(Uptane::RepositoryType::Director(), data_json, root);
    FAIL() << "Illegal threshold should have thrown an error.";
  } catch (const Uptane::IllegalThreshold &ex) {
  } catch (const Uptane::UnmetThreshold &ex) {
  }
}

/* Get manifest from primary.
 * Get manifest from secondaries. */
TEST(Uptane, AssembleManifestGood) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PackageManager::kNone;
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  Json::Value manifest = sota_client->AssembleManifest()["ecu_version_manifests"];
  EXPECT_EQ(manifest.size(), 2);
  EXPECT_EQ(manifest["testecuserial"]["signed"]["ecu_serial"].asString(), config.provision.primary_ecu_serial);
  EXPECT_EQ(manifest["secondary_ecu_serial"]["signed"]["ecu_serial"].asString(), "secondary_ecu_serial");
  // Manifest should not have an installation result yet.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
  EXPECT_FALSE(manifest["secondary_ecu_serial"]["signed"].isMember("custom"));
}

/* Bad signatures are ignored when assembling the manifest. */
TEST(Uptane, AssembleManifestBad) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
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
      UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  /* Overwrite the secondary's keys on disk. */
  std::string private_key, public_key;
  Crypto::generateKeyPair(ecu_config.key_type, &public_key, &private_key);
  Utils::writeFile(ecu_config.full_client_dir / ecu_config.ecu_private_key, private_key);
  public_key = Utils::readFile("tests/test_data/public.key");
  Utils::writeFile(ecu_config.full_client_dir / ecu_config.ecu_public_key, public_key);

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  Json::Value manifest = sota_client->AssembleManifest()["ecu_version_manifests"];
  EXPECT_EQ(manifest.size(), 1);
  EXPECT_EQ(manifest["testecuserial"]["signed"]["ecu_serial"].asString(), config.provision.primary_ecu_serial);
  // Manifest should not have an installation result yet.
  EXPECT_FALSE(manifest["testecuserial"]["signed"].isMember("custom"));
  EXPECT_FALSE(manifest["secondary_ecu_serial"]["signed"].isMember("custom"));
}

/* Get manifest from primary.
 * Get manifest from secondaries.
 * Send manifest to the server. */
TEST(Uptane, PutManifest) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.provision.primary_ecu_serial = "testecuserial";
  config.pacman.type = PackageManager::kNone;
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);

  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
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
  HttpPutManifestFail(const boost::filesystem::path &test_dir_in) : HttpFake(test_dir_in) {}
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
  auto sota_client = SotaUptaneClient::newTestClient(conf, storage, http, events_channel);
  EXPECT_NO_THROW(sota_client->initialize());
  auto result = sota_client->putManifest();
  EXPECT_FALSE(result);
  EXPECT_EQ(num_events_PutManifestError, 1);
}

unsigned int num_events_Install = 0;
void process_events_Install(const std::shared_ptr<event::BaseEvent> &event) {
  if (event->variant == "InstallTargetComplete") {
    auto concrete_event = std::static_pointer_cast<event::InstallTargetComplete>(event);
    if (num_events_Install == 0) {
      EXPECT_TRUE(concrete_event->success);
    } else {
      EXPECT_FALSE(concrete_event->success);
    }
    num_events_Install++;
  }
}
/*
 * Verify successful installation of a provided fake package. Skip fetching and
 * downloading.
 * Identify ECU for each target.
 * Check if there are updates to install for the primary.
 * Install a binary update on the primary.
 * Store installation result for primary.
 * Store installation result for device.
 * Check if an update is already installed
 */
TEST(Uptane, InstallFake) {
  Config conf("tests/config/basic.toml");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  conf.provision.primary_ecu_serial = "testecuserial";
  conf.provision.primary_ecu_hardware_id = "testecuhwid";
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = http->tls_server;

  auto storage = INvStorage::newStorage(conf.storage);
  auto events_channel = std::make_shared<event::Channel>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_Install;
  events_channel->connect(f_cb);
  auto up = SotaUptaneClient::newTestClient(conf, storage, http, events_channel);
  EXPECT_NO_THROW(up->initialize());
  std::vector<Uptane::Target> packages_to_install = UptaneTestCommon::makePackage("testecuserial", "testecuhwid");
  up->uptaneInstall(packages_to_install);

  // Make sure operation_result and filepath were correctly written and formatted.
  Json::Value manifest = up->AssembleManifest();
  EXPECT_EQ(manifest["ecu_version_manifests"]["testecuserial"]["signed"]["installed_image"]["filepath"].asString(),
            "testecuserial");
  // Verify nothing has installed for the secondary.
  EXPECT_FALSE(manifest["ecu_version_manifests"]["secondary_ecu_serial"]["signed"].isMember("installed_image"));

  EXPECT_EQ(num_events_Install, 1);
  Json::Value installation_report = manifest["installation_report"]["report"];
  EXPECT_EQ(installation_report["result"]["success"].asBool(), true);
  EXPECT_EQ(installation_report["result"]["code"].asString(), "OK");
  EXPECT_EQ(installation_report["items"][0]["ecu"].asString(), "testecuserial");
  EXPECT_EQ(installation_report["items"][0]["result"]["success"].asBool(), true);
  EXPECT_EQ(installation_report["items"][0]["result"]["code"].asString(), "OK");

  // second install
  up->uptaneInstall(packages_to_install);
  EXPECT_EQ(num_events_Install, 2);
  manifest = up->AssembleManifest();
  installation_report = manifest["installation_report"]["report"];
  EXPECT_EQ(installation_report["result"]["success"].asBool(), false);
  EXPECT_EQ(installation_report["result"]["code"].asString(), "testecuhwid:ALREADY_PROCESSED");
  EXPECT_EQ(installation_report["items"][0]["ecu"].asString(), "testecuserial");
  EXPECT_EQ(installation_report["items"][0]["result"]["success"].asBool(), false);
  EXPECT_EQ(installation_report["items"][0]["result"]["code"].asString(), "ALREADY_PROCESSED");
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
  explicit SecondaryInterfaceMock(Uptane::SecondaryConfig sconfig_in)
      : Uptane::SecondaryInterface(std::move(sconfig_in)) {
    std::string private_key, public_key;
    Crypto::generateKeyPair(sconfig.key_type, &public_key, &private_key);
    public_key_ = PublicKey(public_key, sconfig.key_type);
    Json::Value manifest_unsigned;
    manifest_unsigned["key"] = "value";

    std::string b64sig = Utils::toBase64(
        Crypto::Sign(sconfig.key_type, nullptr, private_key, Json::FastWriter().write(manifest_unsigned)));
    Json::Value signature;
    signature["method"] = "rsassa-pss";
    signature["sig"] = b64sig;
    signature["keyid"] = public_key_.KeyId();
    manifest_["signed"] = manifest_unsigned;
    manifest_["signatures"].append(signature);
  }
  PublicKey getPublicKey() { return public_key_; };

  Json::Value getManifest() { return manifest_; }
  MOCK_METHOD1(putMetadata, bool(const Uptane::RawMetaPack &));
  MOCK_METHOD1(getRootVersion, int32_t(bool));
  bool putRoot(const std::string &, bool) { return true; }
  bool sendFirmware(const std::shared_ptr<std::string> &) { return true; }
  PublicKey public_key_;
  Json::Value manifest_;
};

MATCHER_P(matchMeta, meta, "") {
  return (arg.director_root == meta.director_root) && (arg.image_root == meta.image_root) &&
         (arg.director_targets == meta.director_targets) && (arg.image_timestamp == meta.image_timestamp) &&
         (arg.image_snapshot == meta.image_snapshot) && (arg.image_targets == meta.image_targets);
}

/*
 * Send metadata to secondary ECUs
 * Send EcuInstallationStartedReport to server for secondaries
 */
TEST(Uptane, SendMetadataToSeconadry) {
  Config conf("tests/config/basic.toml");
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeEvents>(temp_dir.Path(), "hasupdates");
  conf.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.provision.primary_ecu_hardware_id = "primary_hw";
  conf.uptane.director_server = http->tls_server + "/director";
  conf.uptane.repo_server = http->tls_server + "/repo";
  conf.storage.path = temp_dir.Path();
  conf.tls.server = http->tls_server;

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::SecondaryType::kVirtual;
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
  auto up = SotaUptaneClient::newTestClient(conf, storage, http);

  up->addNewSecondary(sec);
  EXPECT_NO_THROW(up->initialize());
  up->fetchMeta();
  std::vector<Uptane::Target> packages_to_install =
      UptaneTestCommon::makePackage("secondary_ecu_serial", "secondary_hw");

  Uptane::RawMetaPack meta;
  storage->loadLatestRoot(&meta.director_root, Uptane::RepositoryType::Director());
  storage->loadNonRoot(&meta.director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
  storage->loadLatestRoot(&meta.image_root, Uptane::RepositoryType::Image());
  storage->loadNonRoot(&meta.image_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
  storage->loadNonRoot(&meta.image_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());
  storage->loadNonRoot(&meta.image_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());

  EXPECT_CALL(*sec, putMetadata(matchMeta(meta)));
  up->uptaneInstall(packages_to_install);
  EXPECT_TRUE(EcuInstallationStartedReportGot);
}

/* Register secondary ECUs with director. */
TEST(Uptane, UptaneSecondaryAdd) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  Config config;
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.uptane.director_server = http->tls_server + "/director";
  config.uptane.repo_server = http->tls_server + "/repo";
  config.tls.server = http->tls_server;
  config.provision.primary_ecu_serial = "testecuserial";
  config.storage.path = temp_dir.Path();
  config.pacman.type = PackageManager::kNone;
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
  EXPECT_NO_THROW(sota_client->initialize());

  /* Verify the correctness of the metadata sent to the server about the
   * secondary. */
  Json::Value ecu_data = Utils::parseJSONFile(temp_dir / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 2);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.provision.primary_ecu_serial);
  EXPECT_EQ(ecu_data["ecus"][1]["ecu_serial"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(ecu_data["ecus"][1]["hardware_identifier"].asString(), "secondary_hardware");
  EXPECT_EQ(ecu_data["ecus"][1]["clientKey"]["keytype"].asString(), "RSA");
  EXPECT_TRUE(ecu_data["ecus"][1]["clientKey"]["keyval"]["public"].asString().size() > 0);
}

/* Adding multiple secondaries with the same serial throws an error */
TEST(Uptane, UptaneSecondaryAddSameSerial) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  Config config;
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = ProvisionMode::kAutomatic;
  config.pacman.type = PackageManager::kNone;
  config.storage.path = temp_dir.Path();

  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "second_secondary_hardware");

  auto storage = INvStorage::newStorage(config.storage);
  EXPECT_THROW(SotaUptaneClient::newTestClient(config, storage, http), std::runtime_error);
}

/*
 * Identify previously unknown secondaries
 * Identify currently unavailable secondaries
 */
TEST(Uptane, UptaneSecondaryMisconfigured) {
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  auto http = std::make_shared<HttpFake>(temp_dir.Path());
  {
    Config config;
    config.provision.provision_path = temp_dir / "cred.zip";
    config.provision.mode = ProvisionMode::kAutomatic;
    config.pacman.type = PackageManager::kNone;
    config.storage.path = temp_dir.Path();
    UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");

    auto storage = INvStorage::newStorage(config.storage);
    auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
    EXPECT_NO_THROW(sota_client->initialize());

    std::vector<MisconfiguredEcu> ecus;
    storage->loadMisconfiguredEcus(&ecus);
    EXPECT_EQ(ecus.size(), 0);
  }
  {
    Config config;
    config.provision.provision_path = temp_dir / "cred.zip";
    config.provision.mode = ProvisionMode::kAutomatic;
    config.pacman.type = PackageManager::kNone;
    config.storage.path = temp_dir.Path();
    auto storage = INvStorage::newStorage(config.storage);
    UptaneTestCommon::addDefaultSecondary(config, temp_dir, "new_secondary_ecu_serial", "new_secondary_hardware");
    auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
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
      FAIL() << "Unexpected secondary serial in storage: " << ecus[0].serial.ToString();
    }
  }
  {
    Config config;
    config.provision.provision_path = temp_dir / "cred.zip";
    config.provision.mode = ProvisionMode::kAutomatic;
    config.pacman.type = PackageManager::kNone;
    config.storage.path = temp_dir.Path();
    auto storage = INvStorage::newStorage(config.storage);
    UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hardware");
    auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);
    EXPECT_NO_THROW(sota_client->initialize());

    std::vector<MisconfiguredEcu> ecus;
    storage->loadMisconfiguredEcus(&ecus);
    EXPECT_EQ(ecus.size(), 0);
  }
}

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
      /* Register primary ECU with director. */
      ecus_count++;
      EXPECT_EQ(data["primary_ecu_serial"].asString(), "tst149_ecu_serial");
      EXPECT_EQ(data["ecus"][0]["hardware_identifier"].asString(), "tst149_hardware_identifier");
      EXPECT_EQ(data["ecus"][0]["ecu_serial"].asString(), "tst149_ecu_serial");
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

  HttpResponse handle_event(const std::string &url, const Json::Value &data) override {
    (void)url;
    ++events_seen;
    if (events_seen == 1) {
      /* Send EcuInstallationStartedReport to server for primary. */
      EXPECT_EQ(data[0]["eventType"]["id"], "EcuInstallationStarted");
      EXPECT_EQ(data[0]["event"]["ecu"], "tst149_ecu_serial");
    } else if (events_seen == 2) {
      /* Send EcuInstallationCompletedReport to server for primary. */
      EXPECT_EQ(data[0]["eventType"]["id"], "EcuInstallationCompleted");
      EXPECT_EQ(data[0]["event"]["ecu"], "tst149_ecu_serial");
    } else {
      std::cout << "Unexpected event: " << data[0]["eventType"]["id"];
      EXPECT_EQ(0, 1);
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
    } else if (url.find("/core/system_info") != std::string::npos) {
      /* Send hardware info to the server. */
      system_info_count++;
      Json::Value hwinfo = Utils::getHardwareInfo();
      EXPECT_EQ(hwinfo["id"].asString(), data["id"].asString());
      EXPECT_EQ(hwinfo["description"].asString(), data["description"].asString());
      EXPECT_EQ(hwinfo["class"].asString(), data["class"].asString());
      EXPECT_EQ(hwinfo["product"].asString(), data["product"].asString());
    } else if (url.find("/director/manifest") != std::string::npos) {
      /* Get manifest from primary.
       * Get primary installation result.
       * Send manifest to the server. */
      manifest_count++;
      std::string hash;
      if (manifest_count <= 2) {
        // Check for default initial value of packagemanagerfake.
        hash = boost::algorithm::to_lower_copy(boost::algorithm::hex(Crypto::sha256digest("")));
      } else {
        hash = "tst149_ecu_serial";
      }
      EXPECT_EQ(data["signed"]["ecu_version_manifests"]["tst149_ecu_serial"]["signed"]["installed_image"]["filepath"]
                    .asString(),
                (hash == "tst149_ecu_serial") ? hash : "unknown");
      EXPECT_EQ(data["signed"]["ecu_version_manifests"]["tst149_ecu_serial"]["signed"]["installed_image"]["fileinfo"]
                    ["hashes"]["sha256"]
                        .asString(),
                hash);
    } else if (url.find("/system_info/network") != std::string::npos) {
      /* Send networking info to the server. */
      network_count++;
      Json::Value nwinfo = Utils::getNetworkInfo();
      EXPECT_EQ(nwinfo["local_ipv4"].asString(), data["local_ipv4"].asString());
      EXPECT_EQ(nwinfo["mac"].asString(), data["mac"].asString());
      EXPECT_EQ(nwinfo["hostname"].asString(), data["hostname"].asString());
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
};

unsigned int num_events_ProvisionOnServer = 0;
void process_events_ProvisionOnServer(const std::shared_ptr<event::BaseEvent> &event) {
  switch (num_events_ProvisionOnServer) {
    case 0: {
      EXPECT_EQ(event->variant, "SendDeviceDataComplete");
      break;
    }
    default: { std::cout << "Got " << event->variant << "event\n"; }
  }
  num_events_ProvisionOnServer++;
}

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
  config.provision.primary_ecu_hardware_id = "tst149_hardware_identifier";
  config.provision.primary_ecu_serial = "tst149_ecu_serial";
  config.storage.path = temp_dir.Path();

  auto storage = INvStorage::newStorage(config.storage);
  auto events_channel = std::make_shared<event::Channel>();
  std::function<void(std::shared_ptr<event::BaseEvent> event)> f_cb = process_events_ProvisionOnServer;
  events_channel->connect(f_cb);
  auto up = SotaUptaneClient::newTestClient(config, storage, http, events_channel);

  EXPECT_EQ(http->devices_count, 0);
  EXPECT_EQ(http->ecus_count, 0);
  EXPECT_EQ(http->manifest_count, 0);
  EXPECT_EQ(http->installed_count, 0);
  EXPECT_EQ(http->system_info_count, 0);
  EXPECT_EQ(http->network_count, 0);

  EXPECT_NO_THROW(up->initialize());
  EcuSerials serials;
  storage->loadEcuSerials(&serials);
  EXPECT_EQ(serials[0].second.ToString(), "tst149_hardware_identifier");

  EXPECT_EQ(http->devices_count, 1);
  EXPECT_EQ(http->ecus_count, 1);

  EXPECT_NO_THROW(up->sendDeviceData());
  EXPECT_EQ(http->manifest_count, 1);
  EXPECT_EQ(http->installed_count, 1);
  EXPECT_EQ(http->system_info_count, 1);
  EXPECT_EQ(http->network_count, 1);

  // We need to fetch metadata, but we currently expect a target hash mismatch
  // since our update is bogus.
  up->fetchMeta();  // TODO check that it failed with return value
  EXPECT_EQ(http->manifest_count, 2);

  // Testing downloading is not strictly necessary here and will not work due to
  // the metadata concerns anyway.
  std::vector<Uptane::Target> packages_to_install =
      UptaneTestCommon::makePackage(config.provision.primary_ecu_serial, config.provision.primary_ecu_hardware_id);
  result::Download result = up->downloadImages(packages_to_install);
  EXPECT_EQ(result.updates.size(), 0);
  EXPECT_EQ(result.status, result::DownloadStatus::kError);

  // Test installation to make sure the metadata put to the server is correct.
  up->uptaneInstall(packages_to_install);
  up->putManifest();

  EXPECT_EQ(http->devices_count, 1);
  EXPECT_EQ(http->ecus_count, 1);
  EXPECT_EQ(http->manifest_count, 3);
  EXPECT_EQ(http->installed_count, 1);
  EXPECT_EQ(http->system_info_count, 1);
  EXPECT_EQ(http->network_count, 1);
  EXPECT_EQ(http->events_seen, 2);
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

  std::vector<Uptane::Target> installed_versions;
  fs_storage.loadInstalledVersions(&installed_versions, nullptr);

  std::string director_root;
  std::string director_targets;
  std::string images_root;
  std::string images_targets;
  std::string images_timestamp;
  std::string images_snapshot;

  EXPECT_TRUE(fs_storage.loadLatestRoot(&director_root, Uptane::RepositoryType::Director()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_TRUE(fs_storage.loadLatestRoot(&images_root, Uptane::RepositoryType::Image()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&images_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
  EXPECT_TRUE(fs_storage.loadNonRoot(&images_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));

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
  sql_storage->loadPrimaryInstalledVersions(&sql_installed_versions, nullptr, nullptr);

  std::string sql_director_root;
  std::string sql_director_targets;
  std::string sql_images_root;
  std::string sql_images_targets;
  std::string sql_images_timestamp;
  std::string sql_images_snapshot;

  sql_storage->loadLatestRoot(&sql_director_root, Uptane::RepositoryType::Director());
  sql_storage->loadNonRoot(&sql_director_targets, Uptane::RepositoryType::Director(), Uptane::Role::Targets());
  sql_storage->loadLatestRoot(&sql_images_root, Uptane::RepositoryType::Image());
  sql_storage->loadNonRoot(&sql_images_targets, Uptane::RepositoryType::Image(), Uptane::Role::Targets());
  sql_storage->loadNonRoot(&sql_images_timestamp, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp());
  sql_storage->loadNonRoot(&sql_images_snapshot, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot());

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

/* Import a list of installed packages into the storage. */
TEST(Uptane, InstalledVersionImport) {
  Config config;

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

  std::vector<Uptane::Target> installed_versions;
  storage->loadPrimaryInstalledVersions(&installed_versions, nullptr, nullptr);
  EXPECT_EQ(installed_versions.at(0).filename(),
            "master-863de625f305413dc3be306afab7c3f39d8713045cfff812b3af83f9722851f0");

  // check that data is not re-imported later: store new data, reload a new
  // storage with import and see that the new data is still there
  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 123;
  Uptane::Target new_installed_version{"filename", target_json};
  storage->savePrimaryInstalledVersion(new_installed_version, InstalledVersionUpdateMode::kCurrent);

  auto new_storage = INvStorage::newStorage(config.storage);
  new_storage->importData(config.import);

  size_t current_index = SIZE_MAX;
  new_storage->loadPrimaryInstalledVersions(&installed_versions, &current_index, nullptr);
  EXPECT_LT(current_index, installed_versions.size());
  EXPECT_EQ(installed_versions.at(current_index).filename(), "filename");
}

/* Store a list of installed package versions. */
TEST(Uptane, SaveAndLoadVersion) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.provision.device_id = "device_id";
  config.postUpdateValues();
  auto storage = INvStorage::newStorage(config.storage);
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 123;

  Uptane::Target t("target_name", target_json);
  storage->savePrimaryInstalledVersion(t, InstalledVersionUpdateMode::kCurrent);

  std::vector<Uptane::Target> installed_versions;
  storage->loadPrimaryInstalledVersions(&installed_versions, nullptr, nullptr);

  auto f = std::find_if(installed_versions.begin(), installed_versions.end(),
                        [](const Uptane::Target &t_) { return t_.filename() == "target_name"; });
  EXPECT_NE(f, installed_versions.end());
  EXPECT_EQ(f->sha256Hash(), "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d");
  EXPECT_EQ(f->length(), 123);
  EXPECT_EQ(*f, t);
}

/* Verifies that we've prevented bug PRO-5210. This happened when the Root was
 * created from the default constructor (which sets the policy to kRejectAll)
 * and not overwritten by a valid Root from the storage (because the relevant
 * table existed but was empty, which was unexpected). The solution is not to
 * return a default-constructed Root when reading from storage. */
TEST(Uptane, kRejectAllTest) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
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

class HttpFakeUnstable : public HttpFake {
 public:
  HttpFakeUnstable(const boost::filesystem::path &test_dir_in) : HttpFake(test_dir_in, "hasupdates") {}
  HttpResponse get(const std::string &url, int64_t maxsize) override {
    if (unstable_valid_count >= unstable_valid_num) {
      ++unstable_valid_num;
      unstable_valid_count = 0;
      return HttpResponse({}, 503, CURLE_OK, "");
    } else {
      ++unstable_valid_count;
      return HttpFake::get(url, maxsize);
    }
  }

  int unstable_valid_num{0};
  int unstable_valid_count{0};
};

/* Recover from an interrupted Uptane iteration.
 * Fetch metadata from the director.
 * Check metadata from the director.
 * Identify targets for known ECUs.
 * Fetch metadata from the images repo.
 * Check metadata from the images repo. */
TEST(Uptane, restoreVerify) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFakeUnstable>(temp_dir.Path());
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.pacman.type = PackageManager::kNone;
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);

  EXPECT_NO_THROW(sota_client->initialize());
  sota_client->AssembleManifest();
  // 1st attempt, don't get anything
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_FALSE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director()));

  // 2nd attempt, get director root.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));

  // 3rd attempt, get director targets.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Director()));
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Director(), Uptane::Role::Targets()));
  EXPECT_FALSE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Image()));

  // 4th attempt, get images root.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadLatestRoot(nullptr, Uptane::RepositoryType::Image()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));

  // 5th attempt, get images timestamp.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Timestamp()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));

  // 6th attempt, get images snapshot.json
  EXPECT_FALSE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Snapshot()));
  EXPECT_FALSE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));

  // 7th attempt, get images targets.json, successful iteration
  EXPECT_TRUE(sota_client->uptaneIteration());
  EXPECT_TRUE(storage->loadNonRoot(nullptr, Uptane::RepositoryType::Image(), Uptane::Role::Targets()));
}

/* Fetch metadata from the director.
 * Check metadata from the director.
 * Identify targets for known ECUs.
 * Fetch metadata from the images repo.
 * Check metadata from the images repo. */
TEST(Uptane, offlineIteration) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.pacman.type = PackageManager::kNone;
  config.provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
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
/*
 Ignore updates for unrecognized ECUs.
 Reject targets which do not match a known ECU
*/
TEST(Uptane, IgnoreUnknownUpdate) {
  TemporaryDirectory temp_dir;
  auto http = std::make_shared<HttpFake>(temp_dir.Path(), "hasupdates");
  Config config("tests/config/basic.toml");
  config.storage.path = temp_dir.Path();
  config.uptane.director_server = http->tls_server + "director";
  config.uptane.repo_server = http->tls_server + "repo";
  config.pacman.type = PackageManager::kNone;
  config.provision.primary_ecu_serial = "primary_ecu";
  config.provision.primary_ecu_hardware_id = "primary_hw";
  UptaneTestCommon::addDefaultSecondary(config, temp_dir, "secondary_ecu_serial", "secondary_hw");
  config.postUpdateValues();

  auto storage = INvStorage::newStorage(config.storage);
  auto sota_client = SotaUptaneClient::newTestClient(config, storage, http);

  EXPECT_NO_THROW(sota_client->initialize());
  sota_client->AssembleManifest();

  auto result = sota_client->fetchMeta();
  EXPECT_EQ(result.status, result::UpdateStatus::kError);
  EXPECT_STREQ(sota_client->getLastException().what(),
               "The target had an ECU ID that did not match the client's configured ECU id.");
  sota_client->last_exception = Uptane::Exception{"", ""};
  result = sota_client->checkUpdates();
  EXPECT_EQ(result.status, result::UpdateStatus::kError);
  EXPECT_STREQ(sota_client->getLastException().what(),
               "The target had an ECU ID that did not match the client's configured ECU id.");
  std::vector<Uptane::Target> packages_to_install = UptaneTestCommon::makePackage("testecuserial", "testecuhwid");
  sota_client->last_exception = Uptane::Exception{"", ""};
  auto report = sota_client->uptaneInstall(packages_to_install);
  EXPECT_STREQ(sota_client->getLastException().what(),
               "The target had an ECU ID that did not match the client's configured ECU id.");
  EXPECT_EQ(report.ecu_reports.size(), 0);
}

#ifdef BUILD_P11
TEST(Uptane, Pkcs11Provision) {
  Config config;
  TemporaryDirectory temp_dir;
  Utils::createDirectories(temp_dir / "import", S_IRWXU);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir / "import/root.crt");
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
  Initializer initializer(config.provision, storage, http, keys, {});

  EXPECT_TRUE(initializer.isSuccessful());
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
