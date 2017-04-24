#include <gtest/gtest.h>
#include <iostream>
#include <fstream>

#include <string>
#include "boost/algorithm/hex.hpp"

#include "sotauptaneclient.h"
#include "crypto.h"
#include "ostree.h"
std::string test_manifest = "/tmp/test_aktualizr_manifest.txt";
std::string tls_server = "https://tlsserver.com";
std::string metadata_path = "tests/test_data/";


OstreePackage::OstreePackage(const std::string &ecu_serial_in, const std::string &ref_name_in,
                             const std::string &commit_in, const std::string &desc_in, const std::string &treehub_in)
    : ecu_serial(ecu_serial_in), ref_name(ref_name_in), commit(commit_in), description(desc_in), pull_uri(treehub_in) {}

data::InstallOutcome OstreePackage::install(const data::PackageManagerCredentials &cred) {
  (void)cred;
  return data::InstallOutcome(data::OK, "Good");
}


// OstreePackage OstreePackage::fromJson(const std::string &package_str) {
//   Json::Reader reader;
//   Json::Value json;
//   reader.parse(package_str, json);
//   return OstreePackage::fromJson(json);
// }

OstreePackage OstreePackage::fromJson(const Json::Value &json) {
  return OstreePackage(json["ecu_serial"].asString(), json["ref_name"].asString(), json["commit"].asString(),
                       json["description"].asString(), json["pull_uri"].asString());
}

Json::Value OstreePackage::toEcuVersion(const Json::Value &custom) {
  Json::Value installed_image;
  installed_image["filepath"] = ref_name;
  installed_image["fileinfo"]["length"] = 0;
  installed_image["fileinfo"]["hashes"]["sha256"] = commit;
  installed_image["fileinfo"]["custom"] = false;

  Json::Value value;
  value["attacks_detected"] = "";
  value["installed_image"] = installed_image;
  value["ecu_serial"] = ecu_serial;
  value["previous_timeserver_time"] = "1970-01-01T00:00:00Z";
  value["timeserver_time"] = "1970-01-01T00:00:00Z";
  if (custom != Json::nullValue){
    value["custom"] = custom;
  }else{
    value["custom"]  = false;
  }
  return value;
}

OstreePackage OstreePackage::getEcu(const std::string &ecu_serial) {
  return OstreePackage(ecu_serial, "frgfdg", "sfsdf", "dsfsdf", "sfsdfs");
}



HttpClient::HttpClient(){}
void HttpClient::setCerts(const std::string& ca, const std::string& cert, const std::string& pkey) {
  (void)ca;
  (void)cert;
  (void)pkey;
}
HttpClient::~HttpClient(){}
bool HttpClient::authenticate(const AuthConfig &conf){
  (void)conf;
  return true;
}
bool HttpClient::authenticate(const std::string& cert, const std::string& ca_file, const std::string& pkey) {
  (void)ca_file;
  (void)cert;
  (void)pkey;
  
  return true;
}


std::string HttpClient::get(const std::string& url) {
  if (url.find(tls_server) == 0){
    std::string path = url.substr(tls_server.size());
    std::ifstream ks((metadata_path+ path).c_str());
    std::string content((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());
    ks.close();  
    return content;

}
  return url;
}

Json::Value HttpClient::getJson(const std::string& url) {
  Json::Value json;
  Json::Reader reader;
  reader.parse(get(url), json);
  return json;
}

std::string HttpClient::post(const std::string &url, const std::string &data){
  (void)data;
  (void)url;
  
  std::ifstream ks("tests/certs/cred.p12");
  std::string cert_str((std::istreambuf_iterator<char>(ks)), std::istreambuf_iterator<char>());
  ks.close();
  http_code = 200;
  return cert_str;
}


Json::Value HttpClient::post(const std::string& url, const Json::Value& data) {
  (void)url;
  (void)data;
  Json::Value json;
  Json::Reader reader;

  reader.parse("{}", json);
  return json;
}


std::string HttpClient::put(const std::string &url, const std::string &data){
  std::ofstream director_file(test_manifest.c_str());
  director_file << data;
  director_file.close();
  return url;
}

bool HttpClient::download(const std::string &url, const std::string &filename){
  (void)url;
  (void)filename;
  return true;
}


TEST(uptane, get_json) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  
  config.uptane.metadata_path = "tests/test_data/";
  event::Channel *events_channel = new event::Channel();
  SotaUptaneClient up(config, events_channel);
  std::string expected = "B3FBEDF18B9748CD8443762CAEC5610EB373C0CE9CA325E85F7EEC95FAA00BEF";

  std::string result = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(up.getJSON(SotaUptaneClient::Director, "root", false))));
  EXPECT_EQ(expected, result);
  result = boost::algorithm::hex(Crypto::sha256digest(Json::FastWriter().write(up.getJSON(SotaUptaneClient::Repo, "root", false))));
  EXPECT_EQ(expected, result);
}

TEST(uptane, get_endpoint) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  std::string result = up.getEndPointUrl(SotaUptaneClient::Director, "root");
  EXPECT_EQ("https://director.com/root.json", result);
  
  result = up.getEndPointUrl(SotaUptaneClient::Repo, "root");
  EXPECT_EQ("https://repo.com/device_id/root.json", result);
}

TEST(uptane, verify) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  up.initService(SotaUptaneClient::Director);
  SotaUptaneClient::Verified verified;
  bool good = up.verify(SotaUptaneClient::Director, "root", verified, false);
  EXPECT_EQ(good, true);
  EXPECT_EQ(verified.new_version, 1u);
  EXPECT_EQ(verified.old_version, 1u);
}



TEST(uptane, verify_data) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  up.initService(SotaUptaneClient::Director);
  Json::Value data_json = up.getJSON(SotaUptaneClient::Director, "root", false);
  bool good = up.verifyData(SotaUptaneClient::Director, "root", data_json);
  EXPECT_EQ(good, true);
}

TEST(uptane, verify_data_bad) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  Json::Value data_json = up.getJSON(SotaUptaneClient::Director, "root", true);
  data_json.removeMember("signatures");
  bool good = up.verifyData(SotaUptaneClient::Director, "root", data_json);
  EXPECT_EQ(good, false);
}


TEST(uptane, verify_data_unknow_type) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  Json::Value data_json = up.getJSON(SotaUptaneClient::Director, "root", true);
  data_json["signatures"][0]["method"] = "badsignature";
  data_json["signatures"][1]["method"] = "badsignature";
  bool good = up.verifyData(SotaUptaneClient::Director, "root", data_json);
  EXPECT_EQ(good, false);
}

TEST(uptane, verify_data_bed_keyid) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  Json::Value data_json = up.getJSON(SotaUptaneClient::Director, "root", true);
  data_json["signatures"][0]["keyid"] = "badkeyid";
  data_json["signatures"][1]["keyid"] = "badsignature";
  bool good = up.verifyData(SotaUptaneClient::Director, "root", data_json);
  EXPECT_EQ(good, false);
}


TEST(uptane, verify_data_bed_threshold) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  Json::Value data_json = up.getJSON(SotaUptaneClient::Director, "root", true);
  data_json["signatures"][0]["keyid"] = "bedsignature";
  bool good = up.verifyData(SotaUptaneClient::Director, "root", data_json);
  EXPECT_EQ(good, false);
}



TEST(uptane, sign) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.device.certificates_path = "tests/test_data/";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  config.uptane.private_key_path = "priv.key";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  up.initService(SotaUptaneClient::Director);
  Json::Value tosign_json;
  tosign_json["mykey"] = "value";
  Json::Value signed_json = up.sign(tosign_json);

  EXPECT_EQ(signed_json["signed"]["mykey"].asString(), "value");
  EXPECT_EQ(signed_json["signatures"][0]["keyid"].asString(), "00A4C4F1FCB433B2354A523ED13F76708EE0737DC323E1467096251B9A90EEEE");
  EXPECT_EQ(signed_json["signatures"][0]["sig"].asString().size() != 0, true);
}



TEST(SotaUptaneClientTest, device_registered) {
  Config conf;
  conf.updateFromToml("tests/config_tests_prov.toml");
  boost::filesystem::remove(conf.device.certificates_path / conf.tls.client_certificate);
  boost::filesystem::remove(conf.device.certificates_path / conf.tls.ca_file);
  boost::filesystem::remove(conf.device.certificates_path / "bootstrap_ca.pem");
  boost::filesystem::remove(conf.device.certificates_path / "bootstrap_cert.pem");

  event::Channel *events_channel = new event::Channel();
  SotaUptaneClient uptane(conf, events_channel);

  bool result = uptane.deviceRegister();
  EXPECT_EQ(result, true);
  EXPECT_EQ(boost::filesystem::exists(conf.device.certificates_path / conf.tls.client_certificate), true);
  EXPECT_EQ(boost::filesystem::exists(conf.device.certificates_path / conf.tls.ca_file), true);
}

TEST(SotaUptaneClientTest, device_registered_fail) {
  Config conf;
  conf.updateFromToml("tests/config_tests_prov.toml");
  boost::filesystem::remove(conf.device.certificates_path / conf.tls.client_certificate);
  boost::filesystem::remove(conf.device.certificates_path / conf.tls.ca_file);
  boost::filesystem::remove(conf.device.certificates_path / "bootstrap_ca.pem");
  boost::filesystem::remove(conf.device.certificates_path / "bootstrap_cert.pem");
  conf.provision.p12_path = "nonexistent";

  event::Channel *events_channel = new event::Channel();
  SotaUptaneClient uptane(conf, events_channel);

  bool result = uptane.deviceRegister();
  EXPECT_EQ(result, false);

}


TEST(SotaUptaneClientTest, device_registered_putmanifest) {
  Config config;
  config.uptane.metadata_path = "tests/test_data/";
  config.uptane.director_server = "https://director.com";
  config.device.certificates_path = "tests/test_data/";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  
  boost::filesystem::remove(test_manifest);
  up.initService(SotaUptaneClient::Director);

  up.putManifest(SotaUptaneClient::Director);
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

TEST(SotaUptaneClientTest, device_ecu_register) {
  Config config;
  config.uptane.metadata_path = "tests/";
  config.uptane.director_server = "https://director.com";
  config.device.certificates_path = "tests/test_data/";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  config.tls.server = tls_server;

  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  up.ecuRegister();
}

TEST(SotaUptaneClientTest, getAvailableUpdates) {
  Config config;
  config.uptane.metadata_path = "tests/test_dat";
  config.uptane.director_server = tls_server + "/director";
  config.device.certificates_path = "tests/test_data/";
  config.uptane.repo_server = "https://repo.com";
  config.device.uuid = "device_id";
  config.uptane.primary_ecu_serial = "testecuserial";
  config.uptane.private_key_path = "private.key";
  config.tls.server = tls_server;
  event::Channel *events_channel = new event::Channel();

  SotaUptaneClient up(config, events_channel);
  std::vector<OstreePackage> packages =  up.getAvailableUpdates();

}


#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
