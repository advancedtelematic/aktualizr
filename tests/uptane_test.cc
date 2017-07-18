#include <gtest/gtest.h>
#include <fstream>
#include <iostream>

#include <logger.h>
#include <boost/algorithm/hex.hpp>
#include <boost/make_shared.hpp>
#include <string>

#include "crypto.h"
#include "ostree.h"
#include "sotauptaneclient.h"
#include "uptane/uptanerepository.h"
#include "utils.h"

std::string test_manifest = "/tmp/test_aktualizr_manifest.txt";
std::string tls_server = "https://tlsserver.com";
std::string metadata_path = "tests/test_data/";

OstreePackage::OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in,
                             const std::string &desc_in, const std::string &treehub_in)
    : ecu_serial(ecu_serial_in), ref_name(ref_name_in), description(desc_in), pull_uri(treehub_in) {}

data::InstallOutcome OstreePackage::install(const data::PackageManagerCredentials &cred, OstreeConfig config) {
  (void)cred;
  (void)config;
  return data::InstallOutcome(data::OK, "Good");
}

OstreePackage OstreePackage::fromJson(const Json::Value &json) {
  return OstreePackage(json["ecu_serial"].asString(), json["ref_name"].asString(), json["description"].asString(),
                       json["pull_uri"].asString());
}

Json::Value OstreePackage::toEcuVersion(const Json::Value &custom) {
  Json::Value installed_image;
  installed_image["filepath"] = ref_name;
  installed_image["fileinfo"]["length"] = 0;
  installed_image["fileinfo"]["hashes"]["sha256"] = refhash;
  installed_image["fileinfo"]["custom"] = false;

  Json::Value value;
  value["attacks_detected"] = "";
  value["installed_image"] = installed_image;
  value["ecu_serial"] = ecu_serial;
  value["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  value["timeserver_time"] = "1970-01-01T00:00:00Z";
  if (custom != Json::nullValue) {
    value["custom"] = custom;
  } else {
    value["custom"] = false;
  }
  return value;
}

OstreePackage OstreePackage::getEcu(const std::string &ecu_serial,
                                    const std::string &ostree_sysroot __attribute__((unused))) {
  return OstreePackage(ecu_serial, "frgfdg", "dsfsdf", "sfsdfs");
}

Json::Value Ostree::getInstalledPackages(const std::string &file_path) { return Json::Value(); }

HttpClient::HttpClient() {}
void HttpClient::setCerts(const std::string &ca, const std::string &cert, const std::string &pkey) {
  (void)ca;
  (void)cert;
  (void)pkey;
}
HttpClient::~HttpClient() {}
bool HttpClient::authenticate(const AuthConfig &conf) {
  (void)conf;
  return true;
}
bool HttpClient::authenticate(const std::string &cert, const std::string &ca_file, const std::string &pkey) {
  (void)ca_file;
  (void)cert;
  (void)pkey;

  return true;
}

HttpResponse HttpClient::get(const std::string &url) {
  std::cout << "URL:" << url << "\n";
  if (url.find(tls_server) == 0) {
    std::string path = metadata_path + url.substr(tls_server.size());
    std::cout << "filetoopen: " << path << "\n\n\n";
    if (url.find("timestamp.json") != std::string::npos) {
      if (boost::filesystem::exists(path)) {
        boost::filesystem::copy_file(metadata_path + "/timestamp2.json", path,
                                     boost::filesystem::copy_option::overwrite_if_exists);
      } else {
        boost::filesystem::copy_file(metadata_path + "/timestamp1.json", path,
                                     boost::filesystem::copy_option::overwrite_if_exists);
      }
    }
    if (url.find("targets.json") != std::string::npos) {
      Json::Value timestamp = Utils::parseJSONFile(metadata_path + "repo/timestamp.json");
      if (timestamp["signed"]["version"].asInt64() == 2) {
        return HttpResponse(Utils::readFile(metadata_path + "/targets_noupdates.json"), 200, CURLE_OK, "");
      } else {
        return HttpResponse(Utils::readFile(metadata_path + "/targets_hasupdates.json"), 200, CURLE_OK, "");
      }
    }

    std::ifstream ks(path.c_str());
    std::string content((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());
    ks.close();
    return HttpResponse(content, 200, CURLE_OK, "");
  }
  return HttpResponse(url, 200, CURLE_OK, "");
}

HttpResponse HttpClient::post(const std::string &url, const Json::Value &data) {
  (void)url;

  Utils::writeFile("tests/test_data/post.json", data);
  return HttpResponse(Utils::readFile("tests/certs/cred.p12"), 200, CURLE_OK, "");
}

HttpResponse HttpClient::put(const std::string &url, const Json::Value &data) {
  std::ofstream director_file(test_manifest.c_str());
  director_file << data;
  director_file.close();
  return HttpResponse(url, 200, CURLE_OK, "");
}

HttpResponse HttpClient::download(const std::string &url, curl_write_callback callback, void *userp) {
  (void)callback;
  (void)userp;
  std::cout << "URL: " << url << "\n";
  std::string path = metadata_path + url.substr(url.rfind("/targets/") + 9);
  std::cout << "filetoopen: " << path << "\n\n\n";

  std::string content = Utils::readFile(path);

  callback((char *)content.c_str(), content.size(), 1, userp);
  return HttpResponse(content, 200, CURLE_OK, "");
}

bool HttpClient::download(const std::string &url, const std::string &filename) {
  (void)url;
  (void)filename;
  return true;
}

TEST(uptane, get_json) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";

  config.uptane.metadata_path = "tests/test_data/";
  Uptane::TufRepository repo("director", tls_server + "/director", config);
  Uptane::TufRepository repo_repo("repo", tls_server + "/repo", config);

  std::string expected_director = "BAE736B5BB53309D333D56CB766204EA2330691845E6ED982040B1B025359471";
  std::string expected_repo = "BAE736B5BB53309D333D56CB766204EA2330691845E6ED982040B1B025359471";

  std::string result = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(repo.getJSON("root.json"))));
  EXPECT_EQ(expected_director, result);
  result = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(repo_repo.getJSON("root.json"))));
  EXPECT_EQ(expected_repo, result);
}

Uptane::TimeStamp now("2017-01-01T01:00:00Z");

TEST(uptane, verify) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";

  Uptane::TufRepository repo("director", tls_server + "/director", config);
  repo.updateRoot(Uptane::Version());

  repo.verifyRole(Uptane::Role::Root(), now, repo.getJSON("root.json"));
}

TEST(uptane, verify_data_bad) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";

  Uptane::TufRepository repo("director", tls_server + "/director", config);
  Json::Value data_json = repo.getJSON("root");
  data_json.removeMember("signatures");

  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::UnmetThreshold ex) {
  }
}

TEST(uptane, verify_data_unknow_type) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";

  Uptane::TufRepository repo("director", tls_server + "/director", config);
  Json::Value data_json = repo.getJSON("root");
  data_json["signatures"][0]["method"] = "badsignature";
  data_json["signatures"][1]["method"] = "badsignature";

  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::SecurityException ex) {
  }
}

TEST(uptane, verify_data_bed_keyid) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";

  Uptane::TufRepository repo("director", tls_server + "/director", config);
  Json::Value data_json = repo.getJSON("root");
  data_json["signatures"][0]["keyid"] = "badkeyid";
  data_json["signatures"][1]["keyid"] = "badsignature";
  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::SecurityException ex) {
  }
}

TEST(uptane, verify_data_bed_threshold) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = tls_server + "/director";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";

  Uptane::TufRepository repo("director", tls_server + "/director", config);
  Json::Value data_json = repo.getJSON("root");
  data_json["signatures"][0]["keyid"] = "bedsignature";
  try {
    repo.verifyRole(Uptane::Role::Root(), now, data_json);
    FAIL();
  } catch (Uptane::SecurityException ex) {
  }
}

TEST(uptane, sign) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = tls_server + "/director";
  config.device.certificates_directory = "tests/test_data/";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";
  config.uptane.private_key_path = "priv.key";
  config.uptane.public_key_path = "public.key";

  Uptane::Repository uptane_repo(config);

  Json::Value tosign_json;
  tosign_json["mykey"] = "value";

  std::cout << "privatekey full path: " << config.uptane.private_key_path << "\n";
  Json::Value signed_json =
      Crypto::signTuf((config.device.certificates_directory / config.uptane.private_key_path).string(),
                      (config.device.certificates_directory / config.uptane.public_key_path).string(), tosign_json);
  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(),
            "6a809c62b4f6c2ae11abfb260a6a9a57d205fc2887ab9c83bd6be0790293e187");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}

TEST(SotaUptaneClientTest, device_registered) {
  Config conf("tests/config_tests_prov.toml");

  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.client_certificate);
  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.ca_file);
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_ca.pem");
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_cert.pem");

  Uptane::Repository uptane(conf);

  bool result = uptane.deviceRegister();
  EXPECT_EQ(result, true);
  EXPECT_EQ(boost::filesystem::exists(conf.device.certificates_directory / conf.tls.client_certificate), true);
  EXPECT_EQ(boost::filesystem::exists(conf.device.certificates_directory / conf.tls.ca_file), true);
}

TEST(SotaUptaneClientTest, device_registered_fail) {
  Config conf("tests/config_tests_prov.toml");

  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.client_certificate);
  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.ca_file);
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_ca.pem");
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_cert.pem");
  conf.provision.p12_path = "nonexistent";

  Uptane::Repository uptane(conf);

  bool result = uptane.deviceRegister();
  EXPECT_EQ(result, false);
}

TEST(SotaUptaneClientTest, device_registered_putmanifest) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.repo_server = tls_server + "/director";
  config.device.certificates_directory = "tests/test_data/";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";
  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = boost::filesystem::path("mybasedir");
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = "/tmp/firmware.txt";
  config.uptane.secondaries.push_back(ecu_config);

  Uptane::Repository uptane(config);

  boost::filesystem::remove(test_manifest);

  Uptane::Repository uptane_repo(config);

  uptane.putManifest();
  EXPECT_EQ(boost::filesystem::exists(test_manifest), true);
  Json::Value json = Utils::parseJSONFile(test_manifest);

  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 2u);
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][0]["signed"]["ecu_serial"], "secondary_ecu_serial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"][0]["signed"]["installed_image"]["filepath"], "/tmp/firmware.txt");
}

TEST(SotaUptaneClientTest, device_ecu_register) {
  Config config;
  config.uptane.metadata_path = "tests/";
  config.uptane.repo_server = tls_server + "/director";
  config.device.certificates_directory = "tests/test_data/";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";
  config.tls.server = tls_server;

  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";

  Uptane::Repository uptane(config);
  uptane.ecuRegister();
  Json::Value ecu_data = Utils::parseJSONFile("tests/test_data/post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 1);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.uptane.primary_ecu_serial);
}

TEST(SotaUptaneClientTest, RunForeverNoUpdates) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.metadata_path = "tests/test_data";
  conf.uptane.director_server = tls_server + "/director";
  conf.device.certificates_directory = "tests/test_data/";
  conf.uptane.repo_server = tls_server + "/repo";
  conf.device.uuid = "device_id";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.uptane.private_key_path = "private.key";

  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.client_certificate);
  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.ca_file);
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_ca.pem");
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_cert.pem");
  boost::filesystem::remove(metadata_path + "director/timestamp.json");
  boost::filesystem::remove(metadata_path + "repo/timestamp.json");

  conf.tls.server = tls_server;
  event::Channel *events_channel = new event::Channel();
  command::Channel *commands_channel = new command::Channel();

  *commands_channel << boost::make_shared<command::GetUpdateRequests>();
  *commands_channel << boost::make_shared<command::GetUpdateRequests>();
  *commands_channel << boost::make_shared<command::GetUpdateRequests>();
  *commands_channel << boost::make_shared<command::Shutdown>();
  SotaUptaneClient up(conf, events_channel);
  up.runForever(commands_channel);
  if (!events_channel->hasValues()) {
    FAIL();
  }
  boost::shared_ptr<event::BaseEvent> event;
  *events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTargetsUpdated");
  *events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTimestampUpdated");
  *events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTimestampUpdated");
}

TEST(SotaUptaneClientTest, RunForeverHasUpdates) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.metadata_path = "tests/test_data";
  conf.uptane.director_server = tls_server + "/director";
  conf.device.certificates_directory = "tests/test_data/";
  conf.uptane.repo_server = tls_server + "/repo";
  conf.device.uuid = "device_id";
  conf.uptane.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  conf.uptane.private_key_path = "private.key";

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = boost::filesystem::path("mybasedir");
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = "tests/test_data/firmware.txt";
  conf.uptane.secondaries.push_back(ecu_config);

  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.client_certificate);
  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.ca_file);
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_ca.pem");
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_cert.pem");
  boost::filesystem::remove(metadata_path + "director/timestamp.json");
  boost::filesystem::remove(metadata_path + "repo/timestamp.json");

  conf.tls.server = tls_server;
  event::Channel *events_channel = new event::Channel();
  command::Channel *commands_channel = new command::Channel();

  *commands_channel << boost::make_shared<command::GetUpdateRequests>();
  *commands_channel << boost::make_shared<command::Shutdown>();
  SotaUptaneClient up(conf, events_channel);
  up.runForever(commands_channel);
  if (!events_channel->hasValues()) {
    FAIL();
  }
  boost::shared_ptr<event::BaseEvent> event;
  *events_channel >> event;
  EXPECT_EQ(event->variant, "UptaneTargetsUpdated");
  event::UptaneTargetsUpdated *targets_event = static_cast<event::UptaneTargetsUpdated *>(event.get());
  EXPECT_EQ(targets_event->packages.size(), 1u);
  EXPECT_EQ(targets_event->packages[0].ref_name,
            "agl-ota-qemux86-64-a0fb2e119cf812f1aa9e993d01f5f07cb41679096cb4492f1265bff5ac901d0d");
  EXPECT_EQ(Utils::readFile("tests/test_data/firmware.txt"), "This is content");
}

TEST(SotaUptaneClientTest, RunForeverInstall) {
  Config conf("tests/config_tests_prov.toml");
  conf.uptane.primary_ecu_serial = "testecuserial";
  conf.uptane.private_key_path = "private.key";
  conf.uptane.director_server = tls_server + "/director";
  conf.device.certificates_directory = "tests/test_data/";
  conf.uptane.repo_server = tls_server + "/repo";

  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.client_certificate);
  boost::filesystem::remove(conf.device.certificates_directory / conf.tls.ca_file);
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_ca.pem");
  boost::filesystem::remove(conf.device.certificates_directory / "bootstrap_cert.pem");
  boost::filesystem::remove(test_manifest);

  conf.tls.server = tls_server;
  event::Channel *events_channel = new event::Channel();
  command::Channel *commands_channel = new command::Channel();

  std::vector<OstreePackage> packages_to_install;
  packages_to_install.push_back(OstreePackage("test1", "test2", "test3", "test4"));
  *commands_channel << boost::make_shared<command::OstreeInstall>(packages_to_install);
  *commands_channel << boost::make_shared<command::Shutdown>();
  SotaUptaneClient up(conf, events_channel);
  up.runForever(commands_channel);

  EXPECT_EQ(boost::filesystem::exists(test_manifest), true);

  Json::Value json;
  Json::Reader reader;
  std::ifstream ks(test_manifest.c_str());
  std::string mnfst_str((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());

  reader.parse(mnfst_str, json);
  EXPECT_EQ(json["signatures"].size(), 1u);
  EXPECT_EQ(json["signed"]["primary_ecu_serial"].asString(), "testecuserial");
  EXPECT_EQ(json["signed"]["ecu_version_manifest"].size(), 1u);
}

TEST(SotaUptaneClientTest, UptaneSecondaryAdd) {
  Config config;
  config.uptane.metadata_path = "tests/";
  config.uptane.repo_server = tls_server + "/director";
  config.device.certificates_directory = "tests/test_data/";
  config.uptane.repo_server = tls_server + "/repo";
  config.device.uuid = "device_id";
  config.tls.server = tls_server;

  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";

  Uptane::SecondaryConfig ecu_config;
  ecu_config.full_client_dir = boost::filesystem::path("mybasedir");
  ecu_config.ecu_serial = "secondary_ecu_serial";
  ecu_config.ecu_hardware_id = "secondary_hardware";
  ecu_config.ecu_private_key = "sec.priv";
  ecu_config.ecu_public_key = "sec.pub";
  ecu_config.firmware_path = "/tmp/firmware.txt";
  config.uptane.secondaries.push_back(ecu_config);

  Uptane::Repository uptane(config);

  uptane.ecuRegister();
  Json::Value ecu_data = Utils::parseJSONFile("tests/test_data/post.json");
  EXPECT_EQ(ecu_data["ecus"].size(), 2);
  EXPECT_EQ(ecu_data["primary_ecu_serial"].asString(), config.uptane.primary_ecu_serial);
  EXPECT_EQ(ecu_data["ecus"][1]["ecu_serial"].asString(), "secondary_ecu_serial");
  EXPECT_EQ(ecu_data["ecus"][1]["hardware_identifier"].asString(), "secondary_hardware");
  EXPECT_EQ(ecu_data["ecus"][1]["clientKey"]["keytype"].asString(), "RSA");
  EXPECT_TRUE(ecu_data["ecus"][1]["clientKey"]["keyval"]["public"].asString().size() > 0);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif
