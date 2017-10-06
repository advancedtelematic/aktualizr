#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <logger.h>
#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <string>

#include "crypto.h"
#include "fsstorage.h"
#include "httpclient.h"
#include "httpfake.h"
#include "ostree.h"
#include "sotauptaneclient.h"
#include "uptane/uptanerepository.h"
#include "utils.h"

#ifdef BUILD_P11
#include "p11engine.h"
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

const std::string uptane_test_dir = "tests/test_uptane";
const Uptane::TimeStamp now("2017-01-01T01:00:00Z");

TEST(uptane, verify) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);
  repo.updateRoot(Uptane::Version());

  repo.verifyRole(Uptane::Role::Root(), now, repo.getJSON("root.json"));

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(uptane, verify_data_bad) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);
  Json::Value data_json = repo.getJSON("root.json");
  data_json.removeMember("signatures");

  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::UnmetThreshold ex) {
  }

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(uptane, verify_data_unknown_type) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);
  Json::Value data_json = repo.getJSON("root.json");
  data_json["signatures"][0]["method"] = "badsignature";
  data_json["signatures"][1]["method"] = "badsignature";

  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::SecurityException ex) {
  }
}

TEST(uptane, verify_data_bad_keyid) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);
  Json::Value data_json = repo.getJSON("root.json");

  data_json["signatures"][0]["keyid"] = "badkeyid";
  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::UnmetThreshold ex) {
  }

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(uptane, verify_data_bad_threshold) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);
  Json::Value data_json = repo.getJSON("root.json");
  data_json["signed"]["roles"]["root"]["threshold"] = -1;
  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::IllegalThreshold ex) {
  } catch (Uptane::UnmetThreshold ex) {
  }

  boost::filesystem::remove_all(uptane_test_dir);
}

/*
* \verify{\tst{153}} Check that aktualizr creates provisioning files if they
* don't exist already.
*/
TEST(SotaUptaneClientTest, initialize) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.metadata_path = "tests/";
  conf.uptane.repo_server = tls_server + "/director";
  conf.tls.certificates_directory = uptane_test_dir + "/certs";

  conf.uptane.repo_server = tls_server + "/repo";
  conf.tls.server = tls_server;

  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";

  boost::filesystem::remove_all(uptane_test_dir);
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.client_certificate()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.ca_file()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.pkey_file()));

  FSStorage storage(conf);
  std::string pkey;
  std::string cert;
  std::string ca;
  bool result = storage.loadTlsCreds(&ca, &cert, &pkey);
  EXPECT_FALSE(result);
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.client_certificate()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.ca_file()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.pkey_file()));

  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(conf, storage, http);
  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.client_certificate()));
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.ca_file()));
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.pkey_file()));
  Json::Value ecu_data = Utils::parseJSONFile(uptane_test_dir + "/post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 1);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), conf.uptane.primary_ecu_serial);

  boost::filesystem::remove_all(uptane_test_dir);
}

/*
* \verify{\tst{154}} Check that aktualizr does NOT change provisioning files if
* they DO exist already.
*/
TEST(SotaUptaneClientTest, initialize_twice) {
  Config conf("tests/config_tests_prov.toml");
  conf.tls.certificates_directory = uptane_test_dir + "/certs";
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";

  boost::filesystem::remove_all(uptane_test_dir);
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.client_certificate()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.ca_file()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.pkey_file()));

  FSStorage storage(conf);
  std::string pkey1;
  std::string cert1;
  std::string ca1;
  bool result = storage.loadTlsCreds(&ca1, &cert1, &pkey1);
  EXPECT_FALSE(result);
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.client_certificate()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.ca_file()));
  EXPECT_FALSE(boost::filesystem::exists(conf.tls.pkey_file()));

  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(conf, storage, http);
  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.client_certificate()));
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.ca_file()));
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.pkey_file()));

  result = storage.loadTlsCreds(&ca1, &cert1, &pkey1);
  EXPECT_TRUE(result);

  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.client_certificate()));
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.ca_file()));
  EXPECT_TRUE(boost::filesystem::exists(conf.tls.pkey_file()));

  std::string pkey2;
  std::string cert2;
  std::string ca2;
  result = storage.loadTlsCreds(&ca2, &cert2, &pkey2);
  EXPECT_TRUE(result);

  EXPECT_EQ(cert1, cert2);
  EXPECT_EQ(ca1, ca2);
  EXPECT_EQ(pkey1, pkey2);

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
* \verify{\tst{155}} Check that aktualizr generates random ecu_serial for
* primary and all secondaries.
*/
TEST(uptane, random_serial) {
  Config conf_1("tests/config_tests_prov.toml");
  conf_1.tls.certificates_directory = uptane_test_dir + "/certs_1";
  boost::filesystem::remove_all(uptane_test_dir + "/certs_1");
  Config conf_2("tests/config_tests_prov.toml");
  conf_2.tls.certificates_directory = uptane_test_dir + "/certs_2";
  boost::filesystem::remove_all(uptane_test_dir + "/certs_2");

  conf_1.uptane.primary_ecu_serial = "l";
  conf_1.uptane.private_key_path = "private.key";
  conf_1.uptane.public_key_path = "public.key";

  conf_2.uptane.primary_ecu_serial = "";
  conf_2.uptane.private_key_path = "private.key";
  conf_2.uptane.public_key_path = "public.key";

  // add secondaries
  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = uptane_test_dir;
  ecu_config.ecu_serial = "";
  ecu_config.ecu_hardware_id = "";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = uptane_test_dir + "/firmware.txt";
  conf_1.uptane.secondaries.push_back(ecu_config);
  conf_2.uptane.secondaries.push_back(ecu_config);

  FSStorage storage_1(conf_1);
  FSStorage storage_2(conf_2);
  HttpFake http(uptane_test_dir);

  Uptane::Repository uptane_1(conf_1, storage_1, http);
  EXPECT_TRUE(uptane_1.initialize());

  Uptane::Repository uptane_2(conf_2, storage_2, http);
  EXPECT_TRUE(uptane_2.initialize());

  std::vector<std::pair<std::string, std::string> > ecu_serials_1;
  std::vector<std::pair<std::string, std::string> > ecu_serials_2;

  EXPECT_TRUE(storage_1.loadEcuSerials(&ecu_serials_1));
  EXPECT_TRUE(storage_2.loadEcuSerials(&ecu_serials_2));
  EXPECT_EQ(ecu_serials_1.size(), 2);
  EXPECT_EQ(ecu_serials_2.size(), 2);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_NE(ecu_serials_1[1].first, ecu_serials_2[1].first);

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
* \verify{\tst{146}} Check that aktualizr does not generate a pet name when
* device ID is specified. This is currently provisional and not a finalized
* requirement at this time.
*/
TEST(uptane, pet_name_provided) {
  std::string test_name = "test-name-123";
  std::string device_path = uptane_test_dir + "/device_id";

  /* Make sure provided device ID is read as expected. */
  Config conf("tests/config_tests_device_id.toml");
  conf.tls.certificates_directory = uptane_test_dir;
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";

  FSStorage storage(conf);
  HttpFake http(uptane_test_dir);
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

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
* \verify{\tst{145}} Check that aktualizr generates a pet name if no device ID
* is specified.
*/
TEST(uptane, pet_name_creation) {
  std::string device_path = uptane_test_dir + "/device_id";

  // Make sure name is created.
  Config conf("tests/config_tests_prov.toml");
  conf.tls.certificates_directory = uptane_test_dir;
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/cred.zip", uptane_test_dir + "/cred.zip");
  conf.provision.provision_path = uptane_test_dir + "/cred.zip";

  std::string test_name1, test_name2;
  {
    FSStorage storage(conf);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(conf, storage, http);
    EXPECT_TRUE(uptane.initialize());

    EXPECT_TRUE(boost::filesystem::exists(device_path));
    test_name1 = Utils::readFile(device_path);
    EXPECT_NE(test_name1, "");
  }

  // Make sure a new name is generated if the config does not specify a name and
  // there is no device_id file.
  {
    boost::filesystem::remove_all(uptane_test_dir);
    boost::filesystem::create_directory(uptane_test_dir);
    boost::filesystem::copy_file("tests/test_data/cred.zip", uptane_test_dir + "/cred.zip");
    conf.uptane.device_id = "";

    FSStorage storage(conf);
    HttpFake http(uptane_test_dir);
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
    FSStorage storage(conf);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(conf, storage, http);
    EXPECT_TRUE(uptane.initialize());

    EXPECT_TRUE(boost::filesystem::exists(device_path));
    EXPECT_EQ(Utils::readFile(device_path), test_name2);
  }

  // If the device_id file is removed, but the field is still present in the
  // config, re-initializing the config should still read the device_id from
  // config.
  {
    boost::filesystem::remove_all(uptane_test_dir);
    boost::filesystem::create_directory(uptane_test_dir);
    boost::filesystem::copy_file("tests/test_data/cred.zip", uptane_test_dir + "/cred.zip");
    conf.uptane.device_id = test_name2;

    FSStorage storage(conf);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(conf, storage, http);
    EXPECT_TRUE(uptane.initialize());

    EXPECT_TRUE(boost::filesystem::exists(device_path));
    EXPECT_EQ(Utils::readFile(device_path), test_name2);
  }

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
 * \verify{\tst{49}} Check that aktualizr fails on expired methadata
 */
TEST(uptane, expires) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);

  // Check that we don't fail on good metadata.
  EXPECT_NO_THROW(
      repo.verifyRole(Uptane::Role::Targets(), now, Utils::parseJSONFile("tests/test_data/targets_noupdates.json")));

  Uptane::Root root("director", Utils::parseJSONFile("tests/test_data/director/root.json")["signed"]);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_expired.json")),
               Uptane::ExpiredMetadata);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Targets(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/targets_expired.json"), &root),
               Uptane::ExpiredMetadata);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Timestamp(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/timestamp_expired.json"), &root),
               Uptane::ExpiredMetadata);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Snapshot(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/snapshot_expired.json"), &root),
               Uptane::ExpiredMetadata);

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
 * \verify{\tst{52}} Check that aktualizr fails on bad threshold
 */
TEST(uptane, threshold) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);

  // Check that we don't fail on good metadata.
  EXPECT_NO_THROW(
      repo.verifyRole(Uptane::Role::Targets(), now, Utils::parseJSONFile("tests/test_data/targets_noupdates.json")));

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-1.json")),
               Uptane::IllegalThreshold);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-32768.json")),
               Uptane::IllegalThreshold);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-2147483648.json")),
               Uptane::IllegalThreshold);

  EXPECT_THROW(
      repo.verifyRole(Uptane::Role::Root(), now,
                      Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_-9223372036854775808.json")),
      std::runtime_error);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_0.9.json")),
               Uptane::IllegalThreshold);

  EXPECT_THROW(repo.verifyRole(Uptane::Role::Root(), now,
                               Utils::parseJSONFile("tests/test_data/bad_metadata/root_treshold_0.json")),
               Uptane::IllegalThreshold);

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, initialize_fail) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.metadata_path = "tests/";
  conf.uptane.repo_server = tls_server + "/director";
  conf.tls.certificates_directory = uptane_test_dir + "/certs";

  conf.uptane.repo_server = tls_server + "/repo";
  conf.tls.server = tls_server;

  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";

  FSStorage storage(conf);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(conf, storage, http);

  http.provisioningResponse = ProvisionFailure;
  bool result = uptane.initialize();
  http.provisioningResponse = ProvisionOK;
  EXPECT_FALSE(result);
  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, put_manifest) {
  Config config;
  config.uptane.metadata_path = uptane_test_dir;
  config.uptane.repo_server = tls_server + "/director";
  config.tls.certificates_directory = uptane_test_dir;
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/cred.zip", uptane_test_dir + "/cred.zip");
  config.provision.provision_path = uptane_test_dir + "/cred.zip";
  config.provision.mode = kAutomatic;
  config.uptane.repo_server = tls_server + "/repo";
  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";
  config.uptane.public_key_path = "public.key";

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = uptane_test_dir;
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = uptane_test_dir + "/firmware.txt";
  config.uptane.secondaries.push_back(ecu_config);

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());

  Json::Value unsigned_ecu_version =
      OstreePackage::getEcu(config.uptane.primary_ecu_serial, config.ostree.sysroot).toEcuVersion(Json::nullValue);
  uptane.putManifest(uptane.getCurrentVersionManifests(unsigned_ecu_version));

  EXPECT_TRUE(boost::filesystem::exists(uptane_test_dir + test_manifest));
  Json::Value json = Utils::parseJSONFile(uptane_test_dir + test_manifest);

  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 2u);
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][0]["signed"]["ecu_serial"], "secondary_ecu_serial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][0]["signed"]["installed_image"]["filepath"],
            uptane_test_dir + "/firmware.txt");

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, RunForeverNoUpdates) {
  Config conf("tests/config_tests_prov.toml");
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/secondary_firmware.txt", uptane_test_dir + "/secondary_firmware.txt");
  conf.uptane.metadata_path = uptane_test_dir;
  conf.uptane.director_server = tls_server + "/director";
  conf.tls.certificates_directory = uptane_test_dir;
  conf.uptane.repo_server = tls_server + "/repo";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";

  conf.tls.server = tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::Shutdown>();

  FSStorage storage(conf);
  HttpFake http(uptane_test_dir);
  Uptane::Repository repo(conf, storage, http);
  SotaUptaneClient up(conf, &events_channel, repo);
  up.runForever(&commands_channel);

  boost::shared_ptr<event::BaseEvent> event;
  if (!events_channel.hasValues()) {
    FAIL();
  }
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTargetsUpdated");
  if (!events_channel.hasValues()) {
    FAIL();
  }
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTimestampUpdated");
  if (!events_channel.hasValues()) {
    FAIL();
  }
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTimestampUpdated");

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, RunForeverHasUpdates) {
  Config conf("tests/config_tests_prov.toml");
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/secondary_firmware.txt", uptane_test_dir + "/secondary_firmware.txt");
  conf.uptane.metadata_path = uptane_test_dir;
  conf.uptane.director_server = tls_server + "/director";
  conf.tls.certificates_directory = uptane_test_dir;
  conf.uptane.repo_server = tls_server + "/repo";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = uptane_test_dir;
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = uptane_test_dir + "/firmware.txt";
  conf.uptane.secondaries.push_back(ecu_config);

  conf.tls.server = tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::Shutdown>();
  FSStorage storage(conf);
  HttpFake http(uptane_test_dir);
  Uptane::Repository repo(conf, storage, http);
  SotaUptaneClient up(conf, &events_channel, repo);
  up.runForever(&commands_channel);

  boost::shared_ptr<event::BaseEvent> event;
  if (!events_channel.hasValues()) {
    FAIL();
  }
  events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTargetsUpdated");
  event::UptaneTargetsUpdated* targets_event = static_cast<event::UptaneTargetsUpdated*>(event.get());
  EXPECT_EQ(targets_event->packages.size(), 2u);
  EXPECT_EQ(targets_event->packages[0].filename(),
            "agl-ota-qemux86-64-a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d");
  EXPECT_EQ(targets_event->packages[1].filename(), "secondary_firmware.txt");

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, RunForeverInstall) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.public_key_path = "public.key";
  conf.uptane.director_server = tls_server + "/director";
  conf.tls.certificates_directory = uptane_test_dir;
  conf.uptane.repo_server = tls_server + "/repo";

  conf.tls.server = tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  std::vector<Uptane::Target> packages_to_install;
  Json::Value ot_json;
  ot_json["custom"]["ecuIdentifier"] = "testecuserial";
  ot_json["custom"]["targetFormat"] = "OSTREE";
  ot_json["length"] = 10;
  packages_to_install.push_back(Uptane::Target("testostree", ot_json));
  commands_channel << boost::make_shared<command::UptaneInstall>(packages_to_install);
  commands_channel << boost::make_shared<command::Shutdown>();
  FSStorage storage(conf);
  HttpFake http(uptane_test_dir);
  Uptane::Repository repo(conf, storage, http);
  SotaUptaneClient up(conf, &events_channel, repo);
  up.runForever(&commands_channel);

  EXPECT_TRUE(boost::filesystem::exists(uptane_test_dir + test_manifest));

  Json::Value json;
  Json::Reader reader;
  std::ifstream ks((uptane_test_dir + test_manifest).c_str());
  std::string mnfst_str((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());

  reader.parse(mnfst_str, json);
  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 1u);

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, UptaneSecondaryAdd) {
  Config config;
  config.uptane.metadata_path = "tests";
  config.uptane.repo_server = tls_server + "/director";
  config.tls.certificates_directory = uptane_test_dir;
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/cred.zip", uptane_test_dir + "/cred.zip");
  config.provision.provision_path = uptane_test_dir + "/cred.zip";
  config.provision.mode = kAutomatic;
  config.uptane.repo_server = tls_server + "/repo";
  config.tls.server = tls_server;

  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";
  config.uptane.public_key_path = "public.key";

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = uptane_test_dir;
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = uptane_test_dir + "/firmware.txt";
  config.uptane.secondaries.push_back(ecu_config);

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
  Json::Value ecu_data = Utils::parseJSONFile(uptane_test_dir + "/post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 2);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.uptane.primary_ecu_serial);
  EXPECT_EQ(ecu_data["ecus"][1]["ecu_serial"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(ecu_data["ecus"][1]["hardware_identifier"].asString(), "secondary_hardware");
  EXPECT_EQ(ecu_data["ecus"][1]["clientKey"]["keytype"].asString(), "RSA");
  EXPECT_TRUE(ecu_data["ecus"][1]["clientKey"]["keyval"]["public"].asString().size() > 0);

  boost::filesystem::remove_all(uptane_test_dir);
}

void initKeyTests(Config& config, Uptane::SecondaryConfig& ecu_config1, Uptane::SecondaryConfig& ecu_config2) {
  config.uptane.metadata_path = "tests";
  config.uptane.repo_server = tls_server + "/director";
  config.tls.certificates_directory = uptane_test_dir;
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/cred.zip", uptane_test_dir + "/cred.zip");
  config.provision.provision_path = uptane_test_dir + "/cred.zip";
  config.provision.mode = kAutomatic;
  config.uptane.repo_server = tls_server + "/repo";
  config.tls.server = tls_server;

  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";
  config.uptane.public_key_path = "public.key";

  ecu_config1.full_client_dir = uptane_test_dir;
  ecu_config1.ecu_serial = "secondary_ecu_serial1";
  ecu_config1.ecu_hardware_id = "secondary_hardware1";
  ecu_config1.ecu_private_key = "sec1.priv";
  ecu_config1.ecu_public_key = "sec1.pub";
  ecu_config1.firmware_path = uptane_test_dir + "/firmware1.txt";
  config.uptane.secondaries.push_back(ecu_config1);

  ecu_config2.full_client_dir = uptane_test_dir;
  ecu_config2.ecu_serial = "secondary_ecu_serial2";
  ecu_config2.ecu_hardware_id = "secondary_hardware2";
  ecu_config2.ecu_private_key = "sec2.priv";
  ecu_config2.ecu_public_key = "sec2.pub";
  ecu_config2.firmware_path = uptane_test_dir + "/firmware2.txt";
  config.uptane.secondaries.push_back(ecu_config2);
}

void checkKeyTests(FSStorage& storage, Uptane::SecondaryConfig& ecu_config1, Uptane::SecondaryConfig& ecu_config2) {
  std::string ca;
  std::string cert;
  std::string pkey;
  EXPECT_TRUE(storage.loadTlsCreds(&ca, &cert, &pkey));
  EXPECT_TRUE(ca.size() > 0);
  EXPECT_TRUE(cert.size() > 0);
  EXPECT_TRUE(pkey.size() > 0);

  std::string primary_public;
  std::string primary_private;
  EXPECT_TRUE(storage.loadPrimaryKeys(&primary_public, &primary_private));
  EXPECT_TRUE(primary_public.size() > 0);
  EXPECT_TRUE(primary_private.size() > 0);

  std::vector<std::pair<std::string, std::string> > ecu_serials;
  EXPECT_TRUE(storage.loadEcuSerials(&ecu_serials));
  EXPECT_EQ(ecu_serials.size(), 3);

  // There is no available public function to fetch the secondaries' public and
  // private keys, so just do it manually here.
  EXPECT_TRUE(boost::filesystem::exists(ecu_config1.full_client_dir / ecu_config1.ecu_public_key));
  EXPECT_TRUE(boost::filesystem::exists(ecu_config1.full_client_dir / ecu_config1.ecu_private_key));
  std::string sec1_public = Utils::readFile((ecu_config1.full_client_dir / ecu_config1.ecu_public_key).string());
  std::string sec1_private = Utils::readFile((ecu_config1.full_client_dir / ecu_config1.ecu_private_key).string());
  EXPECT_TRUE(sec1_public.size() > 0);
  EXPECT_TRUE(sec1_private.size() > 0);
  EXPECT_NE(sec1_public, sec1_private);

  EXPECT_TRUE(boost::filesystem::exists(ecu_config2.full_client_dir / ecu_config2.ecu_public_key));
  EXPECT_TRUE(boost::filesystem::exists(ecu_config2.full_client_dir / ecu_config2.ecu_private_key));
  std::string sec2_public = Utils::readFile((ecu_config2.full_client_dir / ecu_config2.ecu_public_key).string());
  std::string sec2_private = Utils::readFile((ecu_config2.full_client_dir / ecu_config2.ecu_private_key).string());
  EXPECT_TRUE(sec2_public.size() > 0);
  EXPECT_TRUE(sec2_private.size() > 0);
  EXPECT_NE(sec2_public, sec2_private);

  EXPECT_NE(sec1_public, sec2_public);
  EXPECT_NE(sec1_private, sec2_private);
}

/**
 * \verify{\tst{159}} Check that all keys are present after successful
 * provisioning.
 */
TEST(SotaUptaneClientTest, CheckAllKeys) {
  Config config;
  Uptane::SecondaryConfig ecu_config1;
  Uptane::SecondaryConfig ecu_config2;
  initKeyTests(config, ecu_config1, ecu_config2);

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
  checkKeyTests(storage, ecu_config1, ecu_config2);

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
 * \verify{\tst{160}} Check that aktualizr can recover from a half done device
 * registration.
 */
TEST(SotaUptaneClientTest, RecoverWithoutKeys) {
  Config config;
  Uptane::SecondaryConfig ecu_config1;
  Uptane::SecondaryConfig ecu_config2;
  initKeyTests(config, ecu_config1, ecu_config2);

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
  checkKeyTests(storage, ecu_config1, ecu_config2);

  // Remove TLS keys but keep ECU keys and try to initialize.
  storage.clearTlsCreds();
  EXPECT_TRUE(uptane.initialize());
  checkKeyTests(storage, ecu_config1, ecu_config2);

  // Remove ECU keys but keep TLS keys and try to initialize.
  boost::filesystem::remove(config.tls.certificates_directory / config.uptane.public_key_path);
  boost::filesystem::remove(config.tls.certificates_directory / config.uptane.private_key_path);
  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_public_key);
  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_private_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_public_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_private_key);
  EXPECT_TRUE(uptane.initialize());
  checkKeyTests(storage, ecu_config1, ecu_config2);

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
  * \verify{\tst{149}} Check that basic device info sent by aktualizr on provisioning are on server
*/
TEST(SotaUptaneClientTest, provision_on_server) {
  Config config("tests/config_tests_prov.toml");
  std::string server = "tst149";
  config.uptane.metadata_path = "tests";
  config.tls.certificates_directory = uptane_test_dir;
  config.provision.server = server;
  config.uptane.director_server = server + "/director";
  config.uptane.repo_server = server + "/repo";

  config.uptane.device_id = "tst149_device_id";
  config.uptane.primary_ecu_hardware_id = "tst149_hardware_identifier";
  config.uptane.primary_ecu_serial = "tst149_ecu_serial";

  event::Channel events_channel;
  command::Channel commands_channel;
  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::Shutdown>();
  Uptane::Repository repo(config, storage, http);
  SotaUptaneClient up(config, &events_channel, repo);
  up.runForever(&commands_channel);
  boost::filesystem::remove_all(uptane_test_dir);
}

/**
 * \verify{\tst{185}} Verify that when using implicit provisioning, aktualizr
 * halts if credentials are not available.
 */
TEST(SotaUptaneClientTest, implicit_failure) {
  Config config;
  config.uptane.device_id = "device_id";
  config.tls.certificates_directory = uptane_test_dir;
  config.tls.ca_file_ = "ca.pem";
  config.tls.client_certificate_ = "client.pem";
  config.tls.pkey_file_ = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  boost::filesystem::create_directory(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_FALSE(uptane.initialize());
}

/**
 * \verify{\tst{187}} Verfiy that aktualizr halts when provided incomplete
 * implicit provisioning credentials.
 */
TEST(SotaUptaneClientTest, implicit_incomplete) {
  Config config;
  config.tls.certificates_directory = uptane_test_dir;
  config.tls.ca_file_ = "ca.pem";
  config.tls.client_certificate_ = "client.pem";
  config.tls.pkey_file_ = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);

  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", uptane_test_dir + "/ca.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(uptane_test_dir);
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", uptane_test_dir + "/client.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(uptane_test_dir);
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", uptane_test_dir + "/pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(uptane_test_dir);
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", uptane_test_dir + "/ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", uptane_test_dir + "/client.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(uptane_test_dir);
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", uptane_test_dir + "/ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", uptane_test_dir + "/pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(uptane_test_dir);
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", uptane_test_dir + "/client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", uptane_test_dir + "/pkey.pem");
  EXPECT_FALSE(uptane.initialize());

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
 * \verify{\tst{186}} Verify that aktualizr can implicitly provision with
 * provided credentials.
 */
TEST(SotaUptaneClientTest, implicit_provision) {
  Config config;
  config.tls.certificates_directory = uptane_test_dir;
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", uptane_test_dir + "/ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", uptane_test_dir + "/client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", uptane_test_dir + "/pkey.pem");
  config.tls.ca_file_ = "ca.pem";
  config.tls.client_certificate_ = "client.pem";
  config.tls.pkey_file_ = "pkey.pem";

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, CheckOldProvision) {
  boost::filesystem::path work_dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  boost::filesystem::create_directories(work_dir);
  system((std::string("cp -rf tests/test_data/oldprovdir/* ") + work_dir.string()).c_str());
  Config config;
  config.tls.server = "nonexistent_server";
  config.tls.certificates_directory = work_dir;

  HttpClient http;
  FSStorage storage(config);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());  // It will fail in case of provisioning because of wrong server url
  boost::filesystem::remove_all(work_dir);
}

#ifdef TEST_PKCS11_MODULE_PATH
TEST(SotaUptaneClientTest, pkcs11_provision) {
  Config config;
  config.tls.certificates_directory = uptane_test_dir;
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", uptane_test_dir + "/ca.pem");
  config.tls.ca_file_ = "ca.pem";
  config.p11.module = TEST_PKCS11_MODULE_PATH;
  config.p11.pass = "1234";
  config.tls.client_certificate_ = "01";
  config.tls.cert_source = kPkcs11;
  config.tls.pkey_file_ = "02";
  config.tls.pkey_source = kPkcs11;
  config.uptane.device_id = "cc34f7f3-481d-443b-bceb-e838a36a2d1f";
  config.postUpdateValues();

  FSStorage storage(config);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
  boost::filesystem::remove_all(uptane_test_dir);
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif
