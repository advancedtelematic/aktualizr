#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/polymorphic_pointer_cast.hpp>
#include <boost/shared_ptr.hpp>

#include "crypto.h"
#include "fsstorage.h"
#include "httpclient.h"
#include "httpfake.h"
#include "logger.h"
#include "ostree.h"
#include "sotauptaneclient.h"
#include "uptane/managedsecondary.h"
#include "uptane/tuf.h"
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
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "/";
  FSStorage storage(config.storage);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);
  repo.updateRoot(Uptane::Version());

  repo.verifyRole(Uptane::Role::Root(), now, repo.getJSON("root.json"));

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(uptane, verify_data_bad) {
  Config config;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "/";
  FSStorage storage(config.storage);
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
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "/";
  FSStorage storage(config.storage);
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
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "/";
  FSStorage storage(config.storage);
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
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "/";
  FSStorage storage(config.storage);
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
  conf.uptane.repo_server = tls_server + "/director";

  conf.uptane.repo_server = tls_server + "/repo";
  conf.tls.server = tls_server;

  conf.uptane.primary_ecu_serial = "testecuserial";

  conf.storage.path = uptane_test_dir;
  conf.storage.uptane_metadata_path = "metadata";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_private_key_path = "public.key";

  boost::filesystem::remove_all(uptane_test_dir);
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  FSStorage storage(conf.storage);
  std::string pkey;
  std::string cert;
  std::string ca;
  bool result = storage.loadTlsCreds(&ca, &cert, &pkey);
  EXPECT_FALSE(result);
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(conf, storage, http);
  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));
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
  conf.storage.path = uptane_test_dir;
  conf.storage.uptane_metadata_path = "/";
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";

  boost::filesystem::remove_all(uptane_test_dir);
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  FSStorage storage(conf.storage);
  std::string pkey1;
  std::string cert1;
  std::string ca1;
  bool result = storage.loadTlsCreds(&ca1, &cert1, &pkey1);
  EXPECT_FALSE(result);
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_FALSE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(conf, storage, http);
  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

  result = storage.loadTlsCreds(&ca1, &cert1, &pkey1);
  EXPECT_TRUE(result);

  result = uptane.initialize();
  EXPECT_TRUE(result);
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_clientcert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_cacert_path));
  EXPECT_TRUE(boost::filesystem::exists(conf.storage.path / conf.storage.tls_pkey_path));

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
  conf_1.storage.path = uptane_test_dir + "/certs_1";
  boost::filesystem::remove_all(uptane_test_dir + "/certs_1");
  Config conf_2("tests/config_tests_prov.toml");
  conf_2.storage.path = uptane_test_dir + "/certs_2";
  boost::filesystem::remove_all(uptane_test_dir + "/certs_2");

  conf_1.uptane.primary_ecu_serial = "";
  conf_1.storage.uptane_private_key_path = "private.key";
  conf_1.storage.uptane_public_key_path = "public.key";

  conf_2.uptane.primary_ecu_serial = "";
  conf_2.storage.uptane_private_key_path = "private.key";
  conf_2.storage.uptane_public_key_path = "public.key";

  // add secondaries
  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = uptane_test_dir + "/sec_1";
  ecu_config.ecu_serial = "";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = uptane_test_dir + "/firmware.txt";
  ecu_config.target_name_path = uptane_test_dir + "/firmware_name.txt";
  ecu_config.metadata_path = uptane_test_dir + "/secondary_metadata";
  conf_1.uptane.secondary_configs.push_back(ecu_config);
  ecu_config.full_client_dir = uptane_test_dir + "/sec_2";
  conf_2.uptane.secondary_configs.push_back(ecu_config);

  FSStorage storage_1(conf_1.storage);
  FSStorage storage_2(conf_2.storage);
  HttpFake http(uptane_test_dir);

  Uptane::Repository uptane_1(conf_1, storage_1, http);
  SotaUptaneClient uptane_client1(conf_1, NULL, uptane_1);
  EXPECT_TRUE(uptane_1.initialize());

  Uptane::Repository uptane_2(conf_2, storage_2, http);
  SotaUptaneClient uptane_client2(conf_2, NULL, uptane_2);
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
  conf.storage.path = uptane_test_dir;
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";
  conf.uptane.primary_ecu_serial = "testecuserial";

  FSStorage storage(conf.storage);
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
  conf.storage.path = uptane_test_dir;
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";
  conf.uptane.primary_ecu_serial = "testecuserial";
  boost::filesystem::create_directory(uptane_test_dir);
  boost::filesystem::copy_file("tests/test_data/cred.zip", uptane_test_dir + "/cred.zip");
  conf.provision.provision_path = uptane_test_dir + "/cred.zip";

  std::string test_name1, test_name2;
  {
    FSStorage storage(conf.storage);
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

    FSStorage storage(conf.storage);
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
    FSStorage storage(conf.storage);
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

    FSStorage storage(conf.storage);
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
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "/";

  FSStorage storage(config.storage);
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
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "/";

  FSStorage storage(config.storage);
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
  conf.uptane.repo_server = tls_server + "/director";
  conf.storage.path = uptane_test_dir;
  conf.storage.uptane_metadata_path = "tests";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";

  conf.uptane.repo_server = tls_server + "/repo";
  conf.tls.server = tls_server;

  conf.uptane.primary_ecu_serial = "testecuserial";

  FSStorage storage(conf.storage);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(conf, storage, http);

  http.provisioningResponse = ProvisionFailure;
  bool result = uptane.initialize();
  http.provisioningResponse = ProvisionOK;
  EXPECT_FALSE(result);
  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(SotaUptaneClientTest, put_manifest) {
  TemporaryDirectory temp_dir;
  Config config;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = temp_dir.Path();
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_public_key_path = "public.key";
  boost::filesystem::copy_file("tests/test_data/cred.zip", (temp_dir / "cred.zip").string());
  boost::filesystem::copy_file("tests/test_data/firmware.txt", (temp_dir / "firmware.txt").string());
  boost::filesystem::copy_file("tests/test_data/firmware_name.txt", (temp_dir / "firmware_name.txt").string());
  config.provision.provision_path = (temp_dir / "cred.zip").string();
  config.provision.mode = kAutomatic;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.uptane.primary_ecu_serial = "testecuserial";

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

  FSStorage storage(config.storage);
  HttpFake http(temp_dir.PathString());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());

  Json::Value unsigned_ecu_version =
      OstreePackage("branch-hash", "hash", "").toEcuVersion(config.uptane.primary_ecu_serial, Json::nullValue);

  event::Channel events_channel;
  SotaUptaneClient sota_client(config, &events_channel, uptane);

  uptane.putManifest(sota_client.AssembleManifest());

  EXPECT_TRUE(boost::filesystem::exists(temp_dir / test_manifest));
  Json::Value json = Utils::parseJSONFile((temp_dir / test_manifest).string());

  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 2u);
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][1]["signed"]["ecu_serial"], "secondary_ecu_serial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][1]["signed"]["installed_image"]["filepath"], "test-package");
}

TEST(SotaUptaneClientTest, RunForeverNoUpdates) {
  Config conf("tests/config_tests_prov.toml");
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/secondary_firmware.txt", temp_dir / "secondary_firmware.txt");
  conf.uptane.director_server = tls_server + "/director";
  conf.uptane.repo_server = tls_server + "/repo";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = "/";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";

  conf.tls.server = tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::Shutdown>();

  FSStorage storage(conf.storage);
  HttpFake http(temp_dir.PathString());
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
}

TEST(SotaUptaneClientTest, RunForeverHasUpdates) {
  Config conf("tests/config_tests_prov.toml");
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/secondary_firmware.txt", temp_dir / "secondary_firmware.txt");
  conf.uptane.director_server = tls_server + "/director";
  conf.uptane.repo_server = tls_server + "/repo";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.storage.path = temp_dir.Path();
  conf.storage.uptane_metadata_path = temp_dir.Path();
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";

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

  conf.tls.server = tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  commands_channel << boost::make_shared<command::GetUpdateRequests>();
  commands_channel << boost::make_shared<command::Shutdown>();
  FSStorage storage(conf.storage);
  HttpFake http(temp_dir.PathString());
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
}

TEST(SotaUptaneClientTest, RunForeverInstall) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.director_server = tls_server + "/director";
  conf.uptane.repo_server = tls_server + "/repo";
  conf.storage.path = uptane_test_dir;
  conf.storage.uptane_metadata_path = "/";
  conf.storage.uptane_private_key_path = "private.key";
  conf.storage.uptane_public_key_path = "public.key";

  conf.tls.server = tls_server;
  event::Channel events_channel;
  command::Channel commands_channel;

  std::vector<Uptane::Target> packages_to_install;
  Json::Value ot_json;
  ot_json["custom"]["ecuIdentifier"] = "testecuserial";
  ot_json["custom"]["targetFormat"] = "OSTREE";
  ot_json["length"] = 10;
  packages_to_install.push_back(Uptane::Target("testostree-hash", ot_json));
  commands_channel << boost::make_shared<command::UptaneInstall>(packages_to_install);
  commands_channel << boost::make_shared<command::Shutdown>();
  FSStorage storage(conf.storage);
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
  config.uptane.repo_server = tls_server + "/director";
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.provision_path = (temp_dir / "cred.zip").string();
  config.provision.mode = kAutomatic;
  config.uptane.repo_server = tls_server + "/repo";
  config.tls.server = tls_server;

  config.uptane.primary_ecu_serial = "testecuserial";

  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "tests";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_public_key_path = "public.key";

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

  FSStorage storage(config.storage);
  HttpFake http(temp_dir.PathString());
  Uptane::Repository uptane(config, storage, http);
  event::Channel events_channel;
  SotaUptaneClient sota_client(config, &events_channel, uptane);
  EXPECT_TRUE(uptane.initialize());
  Json::Value ecu_data = Utils::parseJSONFile(temp_dir / "post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 2);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.uptane.primary_ecu_serial);
  EXPECT_EQ(ecu_data["ecus"][1]["ecu_serial"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(ecu_data["ecus"][1]["hardware_identifier"].asString(), "secondary_hardware");
  EXPECT_EQ(ecu_data["ecus"][1]["clientKey"]["keytype"].asString(), "RSA");
  EXPECT_TRUE(ecu_data["ecus"][1]["clientKey"]["keyval"]["public"].asString().size() > 0);
}

void initKeyTests(Config& config, Uptane::SecondaryConfig& ecu_config1, Uptane::SecondaryConfig& ecu_config2,
                  TemporaryDirectory& temp_dir) {
  config.uptane.repo_server = tls_server + "/director";
  boost::filesystem::copy_file("tests/test_data/cred.zip", temp_dir / "cred.zip");
  config.provision.provision_path = (temp_dir / "cred.zip").string();
  config.provision.mode = kAutomatic;
  config.uptane.repo_server = tls_server + "/repo";
  config.tls.server = tls_server;

  config.uptane.primary_ecu_serial = "testecuserial";

  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "tests";
  config.storage.uptane_private_key_path = "private.key";
  config.storage.uptane_public_key_path = "public.key";

  ecu_config1.secondary_type = Uptane::kVirtual;
  ecu_config1.partial_verifying = false;
  ecu_config1.full_client_dir = temp_dir.Path();
  ecu_config1.ecu_serial = "secondary_ecu_serial1";
  ecu_config1.ecu_hardware_id = "secondary_hardware1";
  ecu_config1.ecu_private_key = "sec1.priv";
  ecu_config1.ecu_public_key = "sec1.pub";
  ecu_config1.firmware_path = temp_dir / "firmware1.txt";
  ecu_config1.target_name_path = temp_dir / "firmware1_name.txt";
  ecu_config1.metadata_path = temp_dir / "secondary1_metadata";
  config.uptane.secondary_configs.push_back(ecu_config1);

  ecu_config2.secondary_type = Uptane::kVirtual;
  ecu_config2.partial_verifying = false;
  ecu_config2.full_client_dir = temp_dir.Path();
  ecu_config2.ecu_serial = "secondary_ecu_serial2";
  ecu_config2.ecu_hardware_id = "secondary_hardware2";
  ecu_config2.ecu_private_key = "sec2.priv";
  ecu_config2.ecu_public_key = "sec2.pub";
  ecu_config2.firmware_path = temp_dir / "firmware2.txt";
  ecu_config2.target_name_path = temp_dir / "firmware2_name.txt";
  ecu_config2.metadata_path = temp_dir / "secondary2_metadata";
  config.uptane.secondary_configs.push_back(ecu_config2);
}

void checkKeyTests(FSStorage& storage, SotaUptaneClient& sota_client) {
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

  std::vector<std::string> public_keys;
  std::vector<std::string> private_keys;
  std::map<std::string, boost::shared_ptr<Uptane::SecondaryInterface> >::iterator it;
  for (it = sota_client.secondaries.begin(); it != sota_client.secondaries.end(); it++) {
    if (it->second->sconfig.secondary_type != Uptane::kVirtual &&
        it->second->sconfig.secondary_type != Uptane::kLegacy) {
      continue;
    }
    boost::shared_ptr<Uptane::ManagedSecondary> managed =
        boost::polymorphic_pointer_downcast<Uptane::ManagedSecondary>(it->second);
    std::string public_key;
    std::string private_key;
    EXPECT_TRUE(managed->loadKeys(&public_key, &private_key));
    EXPECT_TRUE(public_key.size() > 0);
    EXPECT_TRUE(private_key.size() > 0);
    EXPECT_NE(public_key, private_key);
    public_keys.push_back(public_key);
    private_keys.push_back(private_key);
  }
  std::sort(public_keys.begin(), public_keys.end());
  EXPECT_EQ(adjacent_find(public_keys.begin(), public_keys.end()), public_keys.end());
  std::sort(private_keys.begin(), private_keys.end());
  EXPECT_EQ(adjacent_find(private_keys.begin(), private_keys.end()), private_keys.end());
}

/**
 * \verify{\tst{159}} Check that all keys are present after successful
 * provisioning.
 */
TEST(SotaUptaneClientTest, CheckAllKeys) {
  Config config;
  Uptane::SecondaryConfig ecu_config1;
  Uptane::SecondaryConfig ecu_config2;
  TemporaryDirectory temp_dir;
  initKeyTests(config, ecu_config1, ecu_config2, temp_dir);

  FSStorage storage(config.storage);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  event::Channel events_channel;
  SotaUptaneClient sota_client(config, &events_channel, uptane);
  EXPECT_TRUE(uptane.initialize());
  checkKeyTests(storage, sota_client);

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
  TemporaryDirectory temp_dir;
  initKeyTests(config, ecu_config1, ecu_config2, temp_dir);

  {
    FSStorage storage(config.storage);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(config, storage, http);
    event::Channel events_channel;
    SotaUptaneClient sota_client(config, &events_channel, uptane);

    EXPECT_TRUE(uptane.initialize());
    checkKeyTests(storage, sota_client);

    // Remove TLS keys but keep ECU keys and try to initialize.
    storage.clearTlsCreds();
  }
  {
    FSStorage storage(config.storage);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(config, storage, http);
    event::Channel events_channel;
    SotaUptaneClient sota_client(config, &events_channel, uptane);

    EXPECT_TRUE(uptane.initialize());
    checkKeyTests(storage, sota_client);
  }

  // Remove ECU keys but keep TLS keys and try to initialize.
  boost::filesystem::remove(config.storage.path / config.storage.uptane_public_key_path);
  boost::filesystem::remove(config.storage.path / config.storage.uptane_private_key_path);
  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_public_key);
  boost::filesystem::remove(ecu_config1.full_client_dir / ecu_config1.ecu_private_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_public_key);
  boost::filesystem::remove(ecu_config2.full_client_dir / ecu_config2.ecu_private_key);

  {
    FSStorage storage(config.storage);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(config, storage, http);
    event::Channel events_channel;
    SotaUptaneClient sota_client(config, &events_channel, uptane);

    EXPECT_TRUE(uptane.initialize());
    checkKeyTests(storage, sota_client);
  }
  boost::filesystem::remove_all(uptane_test_dir);
}

/**
  * \verify{\tst{149}} Check that basic device info sent by aktualizr on provisioning are on server
*/
TEST(SotaUptaneClientTest, provision_on_server) {
  Config config("tests/config_tests_prov.toml");
  std::string server = "tst149";
  config.provision.server = server;
  config.uptane.director_server = server + "/director";
  config.uptane.repo_server = server + "/repo";

  config.uptane.device_id = "tst149_device_id";
  config.uptane.primary_ecu_hardware_id = "tst149_hardware_identifier";
  config.uptane.primary_ecu_serial = "tst149_ecu_serial";

  config.storage.path = uptane_test_dir;
  config.storage.uptane_metadata_path = "tests";

  event::Channel events_channel;
  command::Channel commands_channel;
  FSStorage storage(config.storage);
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

  TemporaryDirectory temp_dir;
  config.storage.path = temp_dir.Path();
  config.storage.uptane_metadata_path = "/";
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.postUpdateValues();

  FSStorage storage(config.storage);
  HttpFake http(temp_dir.PathString());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_FALSE(uptane.initialize());
}

/**
 * \verify{\tst{187}} Verfiy that aktualizr halts when provided incomplete
 * implicit provisioning credentials.
 */
TEST(SotaUptaneClientTest, implicit_incomplete) {
  Config config;
  config.storage.path = uptane_test_dir;
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  FSStorage storage(config.storage);
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
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/implicit/ca.pem", temp_dir / "ca.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/client.pem", temp_dir / "client.pem");
  boost::filesystem::copy_file("tests/test_data/implicit/pkey.pem", temp_dir / "pkey.pem");
  config.storage.path = temp_dir.Path();
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";

  FSStorage storage(config.storage);
  HttpFake http(temp_dir.PathString());
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
}

TEST(SotaUptaneClientTest, CheckOldProvision) {
  TemporaryDirectory work_dir;
  system((std::string("cp -rf tests/test_data/oldprovdir/* ") + work_dir.PathString()).c_str());
  Config config;
  config.tls.server = tls_server;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.storage.path = work_dir.Path();

  HttpFake http(work_dir.PathString(), true);
  FSStorage storage(config.storage);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_FALSE(storage.loadEcuRegistered());
  EXPECT_TRUE(uptane.initialize());
  EXPECT_TRUE(storage.loadEcuRegistered());
}

TEST(SotaUptaneClientTest, save_version) {
  Config config;
  config.storage.path = uptane_test_dir;
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  FSStorage storage(config.storage);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 0;

  Uptane::Target t("target_name", target_json);
  uptane.saveInstalledVersion(t);
  Json::Value result = Utils::parseJSONFile((config.storage.path / "installed_versions").string());
  EXPECT_EQ(result["a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d"].asString(), "target_name");
}

TEST(SotaUptaneClientTest, load_version) {
  Config config;
  config.storage.path = uptane_test_dir;
  config.storage.tls_cacert_path = "ca.pem";
  config.storage.tls_clientcert_path = "client.pem";
  config.storage.tls_pkey_path = "pkey.pem";
  config.uptane.device_id = "device_id";
  config.postUpdateValues();
  FSStorage storage(config.storage);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);

  Json::Value target_json;
  target_json["hashes"]["sha256"] = "a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d";
  target_json["length"] = 0;

  Uptane::Target t("target_name", target_json);
  uptane.saveInstalledVersion(t);

  std::string versions_str;
  storage.loadInstalledVersions(&versions_str);
  Json::Value result = Utils::parseJSON(versions_str);
  EXPECT_EQ(result["a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d"].asString(), "target_name");
}

#ifdef BUILD_P11
TEST(SotaUptaneClientTest, pkcs11_provision) {
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

  FSStorage storage(config.storage);
  HttpFake http(uptane_test_dir);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_TRUE(uptane.initialize());
}
#endif

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif
