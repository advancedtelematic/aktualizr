#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>

#include "fsstorage.h"
#include "httpfake.h"
#include "logger.h"
#include "ostree.h"
#include "sotauptaneclient.h"
#include "uptane/tuf.h"
#include "uptane/uptanerepository.h"
#include "utils.h"

#ifdef BUILD_P11
#include "p11engine.h"
#ifndef TEST_PKCS11_MODULE_PATH
#define TEST_PKCS11_MODULE_PATH "/usr/local/softhsm/libsofthsm2.so"
#endif
#endif

namespace bpo = boost::program_options;
extern bpo::variables_map parse_options(int argc, char* argv[]);
const std::string uptane_test_dir = "tests/test_uptane";
const Uptane::TimeStamp now("2017-01-01T01:00:00Z");
boost::filesystem::path build_dir;

TEST(Uptane, Verify) {
  Config config;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
  FSStorage storage(config.storage);
  HttpFake http(uptane_test_dir);
  Uptane::TufRepository repo("director", tls_server + "/director", config, storage, http);
  repo.updateRoot(Uptane::Version());

  repo.verifyRole(Uptane::Role::Root(), now, repo.getJSON("root.json"));

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(Uptane, VerifyDataBad) {
  Config config;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
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

TEST(Uptane, VerifyDataUnknownType) {
  Config config;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
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

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(Uptane, VerifyDataBadKeyId) {
  Config config;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
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

TEST(Uptane, VerifyDataBadThreshold) {
  Config config;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";

  config.storage.path = uptane_test_dir;
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
TEST(Uptane, Initialize) {
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
TEST(Uptane, InitializeTwice) {
  Config conf("tests/config_tests_prov.toml");
  conf.storage.path = uptane_test_dir;
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
TEST(Uptane, RandomSerial) {
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
  EXPECT_FALSE(ecu_serials_1[0].first.empty());
  EXPECT_FALSE(ecu_serials_1[1].first.empty());
  EXPECT_FALSE(ecu_serials_2[0].first.empty());
  EXPECT_FALSE(ecu_serials_2[1].first.empty());
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_NE(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
 * \verify{\tst{156}} Check that aktualizr saves random ecu_serial for primary
 * and all secondaries.
 */
TEST(Uptane, ReloadSerial) {
  std::vector<std::pair<std::string, std::string> > ecu_serials_1;
  std::vector<std::pair<std::string, std::string> > ecu_serials_2;

  Uptane::SecondaryConfig ecu_config;
  ecu_config.secondary_type = Uptane::kVirtual;
  ecu_config.partial_verifying = false;
  ecu_config.full_client_dir = uptane_test_dir + "/sec";
  ecu_config.ecu_serial = "";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = uptane_test_dir + "/firmware.txt";
  ecu_config.target_name_path = uptane_test_dir + "/firmware_name.txt";
  ecu_config.metadata_path = uptane_test_dir + "/secondary_metadata";

  // Initialize and store serials.
  {
    Config conf("tests/config_tests_prov.toml");
    conf.storage.path = uptane_test_dir;
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";
    conf.uptane.secondary_configs.push_back(ecu_config);

    FSStorage storage(conf.storage);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage.loadEcuSerials(&ecu_serials_1));
    EXPECT_EQ(ecu_serials_1.size(), 2);
    EXPECT_FALSE(ecu_serials_1[0].first.empty());
    EXPECT_FALSE(ecu_serials_1[1].first.empty());
  }

  // Initialize new objects and load serials.
  {
    Config conf("tests/config_tests_prov.toml");
    conf.storage.path = uptane_test_dir;
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";
    conf.uptane.secondary_configs.push_back(ecu_config);

    FSStorage storage(conf.storage);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage.loadEcuSerials(&ecu_serials_2));
    EXPECT_EQ(ecu_serials_2.size(), 2);
    EXPECT_FALSE(ecu_serials_2[0].first.empty());
    EXPECT_FALSE(ecu_serials_2[1].first.empty());
  }

  EXPECT_EQ(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_EQ(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);

  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(Uptane, LegacySerial) {
  bpo::variables_map cmd;
  bpo::options_description description("some text");
  // clang-format off
  description.add_options()
    ("legacy-interface", bpo::value<std::string>()->composing(), "path to legacy secondary ECU interface program");
  // clang-format on
  std::string path = (build_dir / "src/external_secondaries/example-interface").string();
  const char* argv[] = {"aktualizr", "--legacy-interface", path.c_str()};
  bpo::store(bpo::parse_command_line(3, argv, description), cmd);

  std::vector<std::pair<std::string, std::string> > ecu_serials_1;
  std::vector<std::pair<std::string, std::string> > ecu_serials_2;

  // Initialize and store serials.
  {
    Config conf("tests/config_tests_prov.toml", cmd);
    conf.storage.path = uptane_test_dir;
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";

    FSStorage storage(conf.storage);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage.loadEcuSerials(&ecu_serials_1));
    EXPECT_EQ(ecu_serials_1.size(), 3);
    EXPECT_FALSE(ecu_serials_1[0].first.empty());
    EXPECT_FALSE(ecu_serials_1[1].first.empty());
    EXPECT_FALSE(ecu_serials_1[2].first.empty());
  }

  // Initialize new objects and load serials.
  {
    Config conf("tests/config_tests_prov.toml", cmd);
    conf.storage.path = uptane_test_dir;
    conf.uptane.primary_ecu_serial = "";
    conf.storage.uptane_private_key_path = "private.key";
    conf.storage.uptane_public_key_path = "public.key";

    FSStorage storage(conf.storage);
    HttpFake http(uptane_test_dir);
    Uptane::Repository uptane(conf, storage, http);
    SotaUptaneClient uptane_client(conf, NULL, uptane);
    EXPECT_TRUE(uptane.initialize());
    EXPECT_TRUE(storage.loadEcuSerials(&ecu_serials_2));
    EXPECT_EQ(ecu_serials_2.size(), 3);
    EXPECT_FALSE(ecu_serials_2[0].first.empty());
    EXPECT_FALSE(ecu_serials_2[1].first.empty());
    EXPECT_FALSE(ecu_serials_2[2].first.empty());
  }

  EXPECT_EQ(ecu_serials_1[0].first, ecu_serials_2[0].first);
  EXPECT_EQ(ecu_serials_1[1].first, ecu_serials_2[1].first);
  EXPECT_EQ(ecu_serials_1[2].first, ecu_serials_2[2].first);

  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[1].first);
  EXPECT_NE(ecu_serials_1[0].first, ecu_serials_1[2].first);
  EXPECT_NE(ecu_serials_1[1].first, ecu_serials_1[2].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[1].first);
  EXPECT_NE(ecu_serials_2[0].first, ecu_serials_2[2].first);
  EXPECT_NE(ecu_serials_2[1].first, ecu_serials_2[2].first);

  boost::filesystem::remove_all(uptane_test_dir);
}

/**
* \verify{\tst{146}} Check that aktualizr does not generate a pet name when
* device ID is specified. This is currently provisional and not a finalized
* requirement at this time.
*/
TEST(Uptane, PetNameProvided) {
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
TEST(Uptane, PetNameCreation) {
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
TEST(Uptane, Expires) {
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
TEST(Uptane, Threshold) {
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

TEST(Uptane, InitializeFail) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.repo_server = tls_server + "/director";
  conf.storage.path = uptane_test_dir;
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

TEST(Uptane, PutManifest) {
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
  SotaUptaneClient sota_client(config, NULL, uptane);
  EXPECT_TRUE(uptane.initialize());

  EXPECT_TRUE(uptane.putManifest(sota_client.AssembleManifest()));
  EXPECT_TRUE(boost::filesystem::exists(temp_dir / test_manifest));
  Json::Value json = Utils::parseJSONFile((temp_dir / test_manifest).string());

  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 2u);
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][1]["signed"]["ecu_serial"], "secondary_ecu_serial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][1]["signed"]["installed_image"]["filepath"], "test-package");
}

TEST(Uptane, RunForeverNoUpdates) {
  Config conf("tests/config_tests_prov.toml");
  TemporaryDirectory temp_dir;
  boost::filesystem::copy_file("tests/test_data/secondary_firmware.txt", temp_dir / "secondary_firmware.txt");
  conf.uptane.director_server = tls_server + "/director";
  conf.uptane.repo_server = tls_server + "/repo";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.storage.path = temp_dir.Path();
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

TEST(Uptane, RunForeverHasUpdates) {
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

TEST(Uptane, RunForeverInstall) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.director_server = tls_server + "/director";
  conf.uptane.repo_server = tls_server + "/repo";
  conf.storage.path = uptane_test_dir;
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

TEST(Uptane, UptaneSecondaryAdd) {
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

/**
 * \verify{\tst{149}} Check that basic device info sent by aktualizr on provisioning are on server
 */
TEST(Uptane, ProvisionOnServer) {
  Config config("tests/config_tests_prov.toml");
  std::string server = "tst149";
  config.provision.server = server;
  config.uptane.director_server = server + "/director";
  config.uptane.repo_server = server + "/repo";

  config.uptane.device_id = "tst149_device_id";
  config.uptane.primary_ecu_hardware_id = "tst149_hardware_identifier";
  config.uptane.primary_ecu_serial = "tst149_ecu_serial";

  config.storage.path = uptane_test_dir;

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

TEST(Uptane, CheckOldProvision) {
  TemporaryDirectory temp_dir;
  system((std::string("cp -rf tests/test_data/oldprovdir/* ") + temp_dir.PathString()).c_str());
  Config config;
  config.tls.server = tls_server;
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.storage.path = temp_dir.PathString();

  HttpFake http(temp_dir.PathString(), true);
  FSStorage storage(config.storage);
  Uptane::Repository uptane(config, storage, http);
  EXPECT_FALSE(storage.loadEcuRegistered());
  EXPECT_TRUE(uptane.initialize());
  EXPECT_TRUE(storage.loadEcuRegistered());
}

TEST(Uptane, SaveVersion) {
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
  boost::filesystem::remove_all(uptane_test_dir);
}

TEST(Uptane, LoadVersion) {
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

  std::map<std::string, std::string> versions;
  storage.loadInstalledVersions(&versions);
  EXPECT_EQ(versions["a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d"], "target_name");
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

  if (argc != 2) {
    return EXIT_FAILURE;
  }
  build_dir = argv[1];
  return RUN_ALL_TESTS();
}
#endif
