/**
 * \file
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include "fsstorage.h"
#include "httpfake.h"
#include "logging.h"
#include "sotauptaneclient.h"
#include "test_utils.h"
#include "uptane/tuf.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"
#include "utils.h"

#ifdef BUILD_P11
#include "p11engine.h"
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

boost::filesystem::path sysroot;

const Uptane::TimeStamp now("2017-01-01T01:00:00Z");

TEST(Uptane, Verify) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::TufRepository repo("director", http.tls_server + "/director", config, storage);
  HttpResponse response = http.get(http.tls_server + "/director/root.json");
  repo.verifyRole(Uptane::Role::Root(), response.getJson());
}

TEST(Uptane, VerifyDataBad) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::TufRepository repo("director", http.tls_server + "/director", config, storage);
  Json::Value data_json = http.get(http.tls_server + "/director/root.json").getJson();
  data_json.removeMember("signatures");

  try {
    repo.verifyRole(Uptane::Role::Root(), data_json);
    FAIL();
  } catch (Uptane::UnmetThreshold ex) {
  }
}

TEST(Uptane, VerifyDataUnknownType) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::TufRepository repo("director", http.tls_server + "/director", config, storage);
  Json::Value data_json = http.get(http.tls_server + "/director/root.json").getJson();
  data_json["signatures"][0]["method"] = "badsignature";
  data_json["signatures"][1]["method"] = "badsignature";

  try {
    repo.verifyRole(Uptane::Role::Root(), data_json);
    FAIL();
  } catch (Uptane::SecurityException ex) {
  }
}

TEST(Uptane, VerifyDataBadKeyId) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::TufRepository repo("director", http.tls_server + "/director", config, storage);
  Json::Value data_json = http.get(http.tls_server + "/director/root.json").getJson();

  data_json["signatures"][0]["keyid"] = "badkeyid";
  try {
    repo.verifyRole(Uptane::Role::Root(), data_json);
    FAIL();
  } catch (Uptane::UnmetThreshold ex) {
  }
}

TEST(Uptane, VerifyDataBadThreshold) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::TufRepository repo("director", http.tls_server + "/director", config, storage);
  Json::Value data_json = http.get(http.tls_server + "/director/root.json").getJson();
  data_json["signed"]["roles"]["root"]["threshold"] = -1;
  try {
    repo.verifyRole(Uptane::Role::Root(), data_json);
    FAIL();
  } catch (Uptane::IllegalThreshold ex) {
  } catch (Uptane::UnmetThreshold ex) {
  }
}

/*
 * \verify{\tst{153}} Check that aktualizr creates provisioning files if they
 * don't exist already.
 */
TEST(Uptane, Initialize) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.repo_server = http.tls_server + "/director";

  conf.uptane.repo_server = http.tls_server + "/repo";
  conf.tls.server = http.tls_server;

  conf.uptane.primary_ecu_serial = "testecuserial";

  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = "metadata";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_private_key_path = "public.key";

  boost::filesystem::remove_all(temp_dir.Path());
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
  std::string pkey;
  std::string cert;
  std::string ca;
  bool result = storage->loadTlsCreds(&ca, &cert, &pkey);
  EXPECT_FALSE(result);
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  Uptane::Repository uptane(conf, storage, http);
  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));
  Json::Value ecu_data = Utils::parseJSONFile(temp_dir.Path() / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 1);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), conf.uptane.primary_ecu_serial);
}

/*
 * \verify{\tst{154}} Check that aktualizr does NOT change provisioning files if
 * they DO exist already.
 */
TEST(Uptane, InitializeTwice) {
  TemporaryDirectory temp_dir;
  Config conf("tests/config_tests_prov.toml");
  conf.storage.path = temp_dir.Path();
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";

  boost::filesystem::remove_all(temp_dir.Path());
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
  std::string pkey1;
  std::string cert1;
  std::string ca1;
  bool result = storage->loadTlsCreds(&ca1, &cert1, &pkey1);
  EXPECT_FALSE(result);
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(conf, storage, http);
  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  result = storage->loadTlsCreds(&ca1, &cert1, &pkey1);
  EXPECT_TRUE(result);

  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  std::string pkey2;
  std::string cert2;
  std::string ca2;
  result = storage->loadTlsCreds(&ca2, &cert2, &pkey2);
  EXPECT_TRUE(result);

  EXPECT_EQ(cert1, cert2);
  EXPECT_EQ(ca1, ca2);
  EXPECT_EQ(pkey1, pkey2);
}

/**
 * \verify{\tst{146}} Check that aktualizr does not generate a pet name when
 * device ID is specified. This is currently provisional and not a finalized
 * requirement at this time.
 */
TEST(Uptane, PetNameProvided) {
  TemporaryDirectory temp_dir;
  std::string test_name = "test-name-123";
  boost::filesystem::path device_path = temp_dir.Path() / "device_id";

  /* Make sure provided device ID is read as expected. */
  Config conf("tests/config_tests_device_id.toml");
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";
  conf.uptane.primary_ecu_serial = "testecuserial";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(conf, storage, http);
  EXPECT_TRUE(uptane.initialize());

  EXPECT_EQ(conf.uptane.device_id, test_name);
  EXPECT_TRUE(boost::filesystem::exists(device_path));
  EXPECT_EQ(Utils::readFile(device_path), test_name);

  /* Make sure name is unchanged after re-initializing config. */
  conf.postUpdateValues();
  EXPECT_EQ(conf.uptane.device_id, test_name);
  EXPECT_TRUE(boost::filesystem::exists(device_path));
  EXPECT_EQ(Utils::readFile(device_path), test_name);
}

/**
 * \verify{\tst{145}} Check that aktualizr generates a pet name if no device ID
 * is specified.
 */
TEST(Uptane, PetNameCreation) {
  TemporaryDirectory temp_dir;
  boost::filesystem::path device_path = temp_dir.Path() / "device_id";

  // Make sure name is created.
  Config conf("tests/config_tests_prov.toml");
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";
  conf.uptane.primary_ecu_serial = "testecuserial";
  boost::filesystem::create_directory(temp_dir.Path());
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir.Path() / "cred.zip");
  conf.provision.provision_path = temp_dir.Path() / "cred.zip";

  std::string test_name1, test_name2;
  {
    boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    EXPECT_TRUE(uptane.initialize());

    EXPECT_TRUE(boost::filesystem::exists(device_path));
    test_name1 = Utils::readFile(device_path);
    EXPECT_NE(test_name1, "");
  }

  // Make sure a new name is generated if the config does not specify a name and
  // there is no device_id file.
  {
    boost::filesystem::remove_all(temp_dir.Path());
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir.Path() / "cred.zip");
    conf.uptane.device_id = "";

    boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    EXPECT_TRUE(uptane.initialize());

    EXPECT_TRUE(boost::filesystem::exists(device_path));
    test_name2 = Utils::readFile(device_path);
    EXPECT_NE(test_name2, test_name1);
  }

  // If the device_id is cleared in the config, but the file is still present,
  // re-initializing the config should still read the device_id from file.
  {
    conf.uptane.device_id = "";
    boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    EXPECT_TRUE(uptane.initialize());

    EXPECT_TRUE(boost::filesystem::exists(device_path));
    EXPECT_EQ(Utils::readFile(device_path), test_name2);
  }

  // If the device_id file is removed, but the field is still present in the
  // config, re-initializing the config should still read the device_id from
  // config.
  {
    boost::filesystem::remove_all(temp_dir.Path());
    boost::filesystem::create_directory(temp_dir.Path());
    boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir.Path() / "cred.zip");
    conf.uptane.device_id = test_name2;

    boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
    HttpFake http(temp_dir.Path());
    Uptane::Repository uptane(conf, storage, http);
    EXPECT_TRUE(uptane.initialize());

    EXPECT_TRUE(boost::filesystem::exists(device_path));
    EXPECT_EQ(Utils::readFile(device_path), test_name2);
  }
}

/**
 * \verify{\tst{49}} Check that aktualizr fails on expired metadata
 */
TEST(Uptane, Expires) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::TufRepository repo("director", http.tls_server + "/director", config, storage);

  // Check that we don't fail on good metadata.
  EXPECT_NO_THROW(
      repo.verifyRole(Uptane::Role::Targets(), Utils::parseJSONFile("tests/test_data/targets_noupdates.json")));

  Uptane::Root root("director", Utils::parseJSONFile("tests/test_data/director/root.json")["signed"]);

  EXPECT_THROW(
      repo.verifyRole(Uptane::Role::Root(), Utils::parseJSONFile("tests/test_data/bad_metadata/root_expired.json")),
      Uptane::ExpiredMetadata);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Targets(),
                               Utils::parseJSONFile("tests/test_data/bad_metadata/targets_expired.json"), &root),
               Uptane::ExpiredMetadata);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Timestamp(),
                               Utils::parseJSONFile("tests/test_data/bad_metadata/timestamp_expired.json"), &root),
               Uptane::ExpiredMetadata);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Snapshot(),
                               Utils::parseJSONFile("tests/test_data/bad_metadata/snapshot_expired.json"), &root),
               Uptane::ExpiredMetadata);
}

/**
 * \verify{\tst{52}} Check that aktualizr fails on bad threshold
 */
TEST(Uptane, Threshold) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";

  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::TufRepository repo("director", http.tls_server + "/director", config, storage);

  // Check that we don't fail on good metadata.
  EXPECT_NO_THROW(
      repo.verifyRole(Uptane::Role::Targets(), Utils::parseJSONFile("tests/test_data/targets_noupdates.json")));

  EXPECT_THROW(
      repo.verifyRole(Uptane::Role::Root(), Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-1.json")),
      Uptane::IllegalThreshold);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(),
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-32768.json")),
               Uptane::IllegalThreshold);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(),
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-2147483648.json")),
               Uptane::IllegalThreshold);

  EXPECT_THROW(
      repo.verifyRole(Uptane::Role::Root(),
                      Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-9223372036854775808.json")),
      std::runtime_error);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(),
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_0.9.json")),
               Uptane::IllegalThreshold);

  EXPECT_THROW(
      repo.verifyRole(Uptane::Role::Root(), Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_0.json")),
      Uptane::IllegalThreshold);
}

TEST(Uptane, InitializeFail) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.repo_server = http.tls_server + "/director";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";

  conf.uptane.repo_server = http.tls_server + "/repo";
  conf.tls.server = http.tls_server;

  conf.uptane.primary_ecu_serial = "testecuserial";

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
  Uptane::Repository uptane(conf, storage, http);

  http.provisioningResponse = ProvisionFailure;
  bool result = uptane.initialize();
  http.provisioningResponse = ProvisionOK;
  EXPECT_FALSE(result);
}

TEST(Uptane, PutManifest) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "metadata";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_public_key_path = "public.key";
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = kAutomatic;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";
  config.uptane.primary_ecu_serial = "testecuserial";
  config.pacman.type = kNone;

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path();
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir / "firmware.txt";
  ecu_config.target_name_path = temp_dir / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir / "secondary_metadata";
  config.uptane.secondary_configs.push_back(ecu_config);

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::Repository uptane(config, storage, http);
  SotaUptaneClient sota_client(config, NULL, uptane, storage, http);
  EXPECT_TRUE(uptane.initialize());

  EXPECT_TRUE(uptane.putManifest(sota_client.AssembleManifest()));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir / http.test_manifest));
  Json::Value json = Utils::parseJSONFile((temp_dir / http.test_manifest).string());

  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 2u);
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][1]["signed"]["ecu_serial"], "secondary_ecu_serial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][1]["signed"]["installed_image"]["filepath"], "test-package");
}

TEST(Uptane, RunForeverNoUpdates) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config conf("tests/config_tests_prov.toml");
  boost::filesystem::copy_file("tests/test_data/secondary_firmware.txt", temp_dir / "secondary_firmware.txt");
  conf.uptane.director_server = http.tls_server + "/director";
  conf.uptane.repo_server = http.tls_server + "/repo";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = "metadata";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";
  conf.pacman.sysroot = sysroot;

  conf.tls.server = http.tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::Shutdown>();

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
  Uptane::Repository repo(conf, storage, http);
  SotaUptaneClient up(conf, &events_channel, repo, storage, http);
  up.runForever(&commands_channel);

  boost::shared_ptr<event::BaseEvent> event;

  EXPECT_TRUE(events_channel.hasValues());
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTargetsUpdated");

  EXPECT_TRUE(events_channel.hasValues());
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTimestampUpdated");

  EXPECT_TRUE(events_channel.hasValues());
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTimestampUpdated");
}

TEST(Uptane, RunForeverHasUpdates) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config conf("tests/config_tests_prov.toml");
  boost::filesystem::copy_file("tests/test_data/secondary_firmware.txt", temp_dir / "secondary_firmware.txt");
  conf.uptane.director_server = http.tls_server + "/director";
  conf.uptane.repo_server = http.tls_server + "/repo";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = "metadata";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";
  conf.pacman.sysroot = sysroot;

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path();
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir / "firmware.txt";
  ecu_config.target_name_path = temp_dir / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir / "secondary_metadata";
  conf.uptane.secondary_configs.push_back(ecu_config);

  conf.tls.server = http.tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::Shutdown>();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
  Uptane::Repository repo(conf, storage, http);
  SotaUptaneClient up(conf, &events_channel, repo, storage, http);
  up.runForever(&commands_channel);

  boost::shared_ptr<event::BaseEvent> event;
  EXPECT_TRUE(events_channel.hasValues());
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTargetsUpdated");
  event::UptaneTargetsUpdated* targets_event = static_cast<event::UptaneTargetsUpdated*>(event.get());
  EXPECT_EQ(targets_event->packages.size(), 2u);
  EXPECT_EQ(targets_event->packages[0].filename(),
            "agl-ota-qemux86-64-a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d");
  EXPECT_EQ(targets_event->packages[1].filename(), "secondary_firmware.txt");
}

std::vector<Uptane::Target> makePackage(const std::string& serial) {
  std::vector<Uptane::Target> packages_to_install;
  Json::Value ot_json;
  ot_json["custom"]["ecuIdentifier"] = serial;
  ot_json["custom"]["targetFormat"] = "OSTREE";
  ot_json["length"] = 0;
  ot_json["hashes"]["sha256"] = serial;
  packages_to_install.push_back(Uptane::Target(std::string("unknown-") + serial, ot_json));
  return packages_to_install;
}

TEST(Uptane, RunForeverInstall) {
  Config conf("tests/config_tests_prov.toml");
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.director_server = http.tls_server + "/director";
  conf.uptane.repo_server = http.tls_server + "/repo";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";
  conf.pacman.sysroot = sysroot;

  conf.tls.server = http.tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  std::vector<Uptane::Target> packages_to_install = makePackage("testostree");
  commands_channel << boost::make_shared<command::UptaneInstall>(packages_to_install);
  commands_channel << boost::make_shared<command::Shutdown>();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(conf.storage);
  Uptane::Repository repo(conf, storage, http);
  SotaUptaneClient up(conf, &events_channel, repo, storage, http);
  up.runForever(&commands_channel);

  EXPECT_TRUE(boost::filesystem::exists(temp_dir.Path() / http.test_manifest));

  Json::Value json;
  Json::Reader reader;
  std::ifstream ks((temp_dir.Path() / http.test_manifest).c_str());
  std::string mnfst_str((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());

  reader.parse(mnfst_str, json);
  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 1u);
}

TEST(Uptane, UptaneSecondaryAdd) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path());
  Config config;
  config.uptane.repo_server = http.tls_server + "/director";
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.provision_path = temp_dir / "cred.zip";
  config.provision.mode = kAutomatic;
  config.uptane.repo_server = http.tls_server + "/repo";
  config.tls.server = http.tls_server;
  config.uptane.primary_ecu_serial = "testecuserial";
  config.storage.path = temp_dir.Path();
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_public_key_path = "public.key";
  config.pacman.type = kNone;

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = temp_dir.Path();
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = temp_dir / "firmware.txt";
  ecu_config.target_name_path = temp_dir / "firmware_name.txt";
  ecu_config.metadata_path = temp_dir / "secondary_metadata";
  config.uptane.secondary_configs.push_back(ecu_config);

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::Repository uptane(config, storage, http);
  event::Channel events_channel;
  SotaUptaneClient sota_client(config, &events_channel, uptane, storage, http);
  EXPECT_TRUE(uptane.initialize());
  Json::Value ecu_data = Utils::parseJSONFile(temp_dir / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 2);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.uptane.primary_ecu_serial);
  EXPECT_EQ(ecu_data["ecus"][1]["ecu_serial"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(ecu_data["ecus"][1]["hardware_identifier"].asString(), "secondary_hardware");
  EXPECT_EQ(ecu_data["ecus"][1]["clientKey"]["keytype"].asString(), "RSA");
  EXPECT_TRUE(ecu_data["ecus"][1]["clientKey"]["keyval"]["public"].asString().size() > 0);
}

/**
 * \verify{\tst{149}} Check that basic device info sent by aktualizr on provisioning are on server
 * Also test that installation works as expected with the fake package manager.
 */
TEST(Uptane, ProvisionOnServer) {
  TemporaryDirectory temp_dir;
  Config config("tests/config_tests_prov.toml");
  std::string server = "tst149";
  config.provision.server = server;
  config.tls.server = server;
  config.uptane.director_server = server + "/director";
  config.uptane.repo_server = server + "/repo";
  config.uptane.device_id = "tst149_device_id";
  config.uptane.primary_ecu_hardware_id = "tst149_hardware_identifier";
  config.uptane.primary_ecu_serial = "tst149_ecu_serial";
  config.storage.path = temp_dir.Path();

  event::Channel events_channel;
  command::Channel commands_channel;
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  HttpFake http(temp_dir.Path());
  std::vector<Uptane::Target> packages_to_install = makePackage(config.uptane.primary_ecu_serial);
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::UptaneInstall>(packages_to_install);
  commands_channel << boost::make_shared<command::Shutdown>();
  Uptane::Repository repo(config, storage, http);
  SotaUptaneClient up(config, &events_channel, repo, storage, http);
  up.runForever(&commands_channel);
}

TEST(Uptane, CheckOldProvision) {
  TemporaryDirectory temp_dir;
  HttpFake http(temp_dir.Path(), true);
  int result = system((std::string("cp -rf tests/test_data/oldprovdir/* ") + temp_dir.PathString()).c_str());
  (void)result;
  Config config;
  config.tls.server = http.tls_server;
  config.uptane.director_server = http.tls_server + "/director";
  config.uptane.repo_server = http.tls_server + "/repo";
  config.storage.path = temp_dir.Path();

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_FALSE(storage->loadEcuRegistered());
  EXPECT_TRUE(uptane.initialize());
  EXPECT_TRUE(storage->loadEcuRegistered());
}

TEST(Uptane, fs_to_sql_full) {
  TemporaryDirectory temp_dir;
  int result = system((std::string("cp -rf tests/test_data/prov/* ") + temp_dir.PathString()).c_str());
  (void)result;
  StorageConfig config;
  config.type = kSqlite;
  config.uptane_metadata_path = "metadata";
  config.path = temp_dir.Path();
  config.sqldb_path = temp_dir.Path() / "database.db";
  config.schemas_path = "config/schemas";

  config.uptane_private_key_path = "ecukey.der";
  config.tls_cacert_path = "root.crt";

  FSStorage fs_storage(config);

  std::string public_key;
  std::string private_key;
  fs_storage.loadPrimaryKeys(&public_key, &private_key);

  std::string ca;
  std::string cert;
  std::string pkey;
  fs_storage.loadTlsCreds(&ca, &cert, &pkey);

  std::string device_id;
  fs_storage.loadDeviceId(&device_id);

  std::vector<std::pair<std::string, std::string> > serials;
  fs_storage.loadEcuSerials(&serials);

  bool ecu_registered = fs_storage.loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> installed_versions;
  fs_storage.loadInstalledVersions(&installed_versions);

  Uptane::MetaPack metadata;
  fs_storage.loadMetadata(&metadata);

  std::cout << "CONFIG.PATH: " << config.path << std::endl;
  std::cout << "CONFIG.UPTANE_PUBLIC_KEY_PATH: " << config.uptane_public_key_path << std::endl;
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_public_key_path)));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_private_key_path)));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, config.tls_cacert_path)));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, config.tls_clientcert_path)));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, config.tls_pkey_path)));

  boost::filesystem::path image_path = Utils::absolutePath(config.path, config.uptane_metadata_path) / "repo";
  boost::filesystem::path director_path = Utils::absolutePath(config.path, config.uptane_metadata_path) / "director";
  EXPECT_TRUE(boost::filesystem::exists(director_path / "root.json"));
  EXPECT_TRUE(boost::filesystem::exists(director_path / "targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(director_path / "root.json"));
  EXPECT_TRUE(boost::filesystem::exists(director_path / "targets.json"));
  EXPECT_TRUE(boost::filesystem::exists(image_path / "timestamp.json"));
  EXPECT_TRUE(boost::filesystem::exists(image_path / "snapshot.json"));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "device_id")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "is_registered")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "primary_ecu_serial")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "primary_ecu_hardware_id")));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, "secondaries_list")));
  boost::shared_ptr<INvStorage> sql_storage = INvStorage::newStorage(config, temp_dir.Path());

  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_public_key_path)));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_private_key_path)));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, config.tls_cacert_path)));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, config.tls_clientcert_path)));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, config.tls_pkey_path)));

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

  std::vector<std::pair<std::string, std::string> > sql_serials;
  sql_storage->loadEcuSerials(&sql_serials);

  bool sql_ecu_registered = sql_storage->loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> sql_installed_versions;
  sql_storage->loadInstalledVersions(&sql_installed_versions);

  Uptane::MetaPack sql_metadata;
  sql_storage->loadMetadata(&sql_metadata);

  EXPECT_EQ(sql_public_key, public_key);
  EXPECT_EQ(sql_private_key, private_key);
  EXPECT_EQ(sql_ca, ca);
  EXPECT_EQ(sql_cert, cert);
  EXPECT_EQ(sql_pkey, pkey);
  EXPECT_EQ(sql_device_id, device_id);
  EXPECT_EQ(sql_serials, serials);
  EXPECT_EQ(sql_ecu_registered, ecu_registered);
  EXPECT_EQ(sql_installed_versions, installed_versions);

  EXPECT_EQ(sql_metadata.director_root, metadata.director_root);
  EXPECT_EQ(sql_metadata.director_targets, metadata.director_targets);
  EXPECT_EQ(sql_metadata.image_root, metadata.image_root);
  EXPECT_EQ(sql_metadata.image_targets, metadata.image_targets);
  EXPECT_EQ(sql_metadata.image_timestamp, metadata.image_timestamp);
  EXPECT_EQ(sql_metadata.image_snapshot, metadata.image_snapshot);
}

TEST(Uptane, fs_to_sql_partial) {
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/prov/ecukey.der", temp_dir.Path() / "ecukey.der");
  boost::filesystem::copy_file("tests/test_data/prov/ecukey.pub", temp_dir.Path() / "ecukey.pub");

  StorageConfig config;
  config.type = kSqlite;
  config.uptane_metadata_path = "metadata";
  config.path = temp_dir.Path();
  config.sqldb_path = temp_dir.Path() / "database.db";
  config.schemas_path = "config/schemas";

  config.uptane_private_key_path = "ecukey.der";
  config.tls_cacert_path = "root.crt";

  FSStorage fs_storage(config);

  std::string public_key;
  std::string private_key;
  fs_storage.loadPrimaryKeys(&public_key, &private_key);

  std::string ca;
  std::string cert;
  std::string pkey;
  fs_storage.loadTlsCreds(&ca, &cert, &pkey);

  std::string device_id;
  fs_storage.loadDeviceId(&device_id);

  std::vector<std::pair<std::string, std::string> > serials;
  fs_storage.loadEcuSerials(&serials);

  bool ecu_registered = fs_storage.loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> installed_versions;
  fs_storage.loadInstalledVersions(&installed_versions);

  Uptane::MetaPack metadata;
  fs_storage.loadMetadata(&metadata);

  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_public_key_path)));
  EXPECT_TRUE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_private_key_path)));

  boost::shared_ptr<INvStorage> sql_storage = INvStorage::newStorage(config, temp_dir.Path());

  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_public_key_path)));
  EXPECT_FALSE(boost::filesystem::exists(Utils::absolutePath(config.path, config.uptane_private_key_path)));

  std::string sql_public_key;
  std::string sql_private_key;
  sql_storage->loadPrimaryKeys(&sql_public_key, &sql_private_key);

  std::string sql_ca;
  std::string sql_cert;
  std::string sql_pkey;
  sql_storage->loadTlsCreds(&sql_ca, &sql_cert, &sql_pkey);

  std::string sql_device_id;
  sql_storage->loadDeviceId(&sql_device_id);

  std::vector<std::pair<std::string, std::string> > sql_serials;
  sql_storage->loadEcuSerials(&sql_serials);

  bool sql_ecu_registered = sql_storage->loadEcuRegistered() ? true : false;

  std::vector<Uptane::Target> sql_installed_versions;
  sql_storage->loadInstalledVersions(&sql_installed_versions);

  Uptane::MetaPack sql_metadata;
  sql_storage->loadMetadata(&sql_metadata);

  EXPECT_EQ(sql_public_key, public_key);
  EXPECT_EQ(sql_private_key, private_key);
  EXPECT_EQ(sql_ca, ca);
  EXPECT_EQ(sql_cert, cert);
  EXPECT_EQ(sql_pkey, pkey);
  EXPECT_EQ(sql_device_id, device_id);
  EXPECT_EQ(sql_serials, serials);
  EXPECT_EQ(sql_ecu_registered, ecu_registered);
  EXPECT_EQ(sql_installed_versions, installed_versions);

  EXPECT_EQ(sql_metadata.director_root, metadata.director_root);
  EXPECT_EQ(sql_metadata.director_targets, metadata.director_targets);
  EXPECT_EQ(sql_metadata.image_root, metadata.image_root);
  EXPECT_EQ(sql_metadata.image_targets, metadata.image_targets);
  EXPECT_EQ(sql_metadata.image_timestamp, metadata.image_timestamp);
  EXPECT_EQ(sql_metadata.image_snapshot, metadata.image_snapshot);
}

TEST(Uptane, SaveVersion) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(config, storage, http);

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 123;

  Uptane::Target t("target_name", target_json);
  storage->saveInstalledVersion(t);
  Json::Value result = Utils::parseJSONFile((config.storage.path / "installed_versions").string());

  EXPECT_EQ(result["target_name"]["hashes"]["sha256"].asString(),
            "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d");
  EXPECT_EQ(result["target_name"]["length"].asInt(), 123);
}

TEST(Uptane, LoadVersion) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(config, storage, http);

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 0;

  Uptane::Target t("target_name", target_json);
  storage->saveInstalledVersion(t);

  std::vector<Uptane::Target> versions;
  storage->loadInstalledVersions(&versions);
  EXPECT_EQ(t, versions[0]);
}

#ifdef BUILD_P11
TEST(Uptane, Pkcs11Provision) {
  Config config;
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir / "ca.pem");
  config.tls.cert_source = kPkcs11;
  config.tls.pkey_source = kPkcs11;
  config.p11.module = TEST_PKCS11_MODULE_PATH;
  config.p11.pass = "1234";
  config.p11.tls_clientcert_id = "01";
  config.p11.tls_pkey_id = "02";

  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = "ca.pem";
  config.postUpdateValues();

  boost::shared_ptr<INvStorage> storage = boost::make_shared<FSStorage>(config.storage);
  HttpFake http(temp_dir.Path());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
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
