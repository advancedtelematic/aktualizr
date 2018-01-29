/**
 * \verify{\tst{158}} Check that aktualizr can complete provisioning after
 * encountering various network issues.
 */
#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <boost/smart_ptr/make_shared.hpp>

#include "fsstorage.h"
#include "httpclient.h"
#include "logging.h"
#include "test_utils.h"
#include "uptane/uptanerepository.h"
#include "utils.h"

Config conf("tests/config_tests_prov.toml");
std::string port;

bool doInit(StorageType storage_type, const std::string &device_register_state, const std::string &ecu_register_state) {
  TemporaryDirectory temp_dir;
  conf.storage.type = storage_type;
  conf.storage.path = temp_dir.Path();
  conf.provision.expiry_days = device_register_state;
  conf.uptane.primary_ecu_serial = ecu_register_state;
  std::string good_url = conf.provision.server;
  if (device_register_state == "noconnection") {
    conf.provision.server = conf.provision.server.substr(conf.provision.server.size() - 2) + "11";
  }

  bool result;
  HttpClient http;
  boost::shared_ptr<INvStorage> fs = boost::make_shared<FSStorage>(conf.storage);
  {
    Uptane::Repository uptane = Uptane::Repository(conf, fs, http);
    result = uptane.initialize();
  }
  if (device_register_state != "noerrors" || conf.uptane.primary_ecu_serial != "noerrors") {
    EXPECT_FALSE(result);
    conf.provision.server = good_url;
    conf.provision.expiry_days = "noerrors";
    conf.uptane.primary_ecu_serial = "noerrors";

    std::vector<std::pair<std::string, std::string> > serials;
    fs->loadEcuSerials(&serials);
    serials[0].first = conf.uptane.primary_ecu_serial;
    fs->storeEcuSerials(serials);

    Uptane::Repository uptane = Uptane::Repository(conf, fs, http);
    result = uptane.initialize();
  }

  return result;
}

// Clang tries to cram these all on single lines, which is ugly.
// clang-format off
TEST(UptaneNetwork, device_drop_request) {
  EXPECT_TRUE(doInit(kFileSystem, "drop_request", "noerrors"));
}

TEST(UptaneNetwork, device_drop_body) {
  EXPECT_TRUE(doInit(kFileSystem, "drop_body", "noerrors"));
}

TEST(UptaneNetwork, device_503) {
  EXPECT_TRUE(doInit(kFileSystem, "status_503", "noerrors"));
}

TEST(UptaneNetwork, device_408) {
  EXPECT_TRUE(doInit(kFileSystem, "status_408", "noerrors"));
}

TEST(UptaneNetwork, ecu_drop_request) {
  EXPECT_TRUE(doInit(kFileSystem, "noerrors", "drop_request"));
}

TEST(UptaneNetwork, ecu_503) {
  EXPECT_TRUE(doInit(kFileSystem, "noerrors", "status_503"));
}

TEST(UptaneNetwork, ecu_408) {
  EXPECT_TRUE(doInit(kFileSystem, "noerrors", "status_408"));
}

TEST(UptaneNetwork, no_connection) {
  EXPECT_TRUE(doInit(kFileSystem, "noconnection", "noerrors"));
}

TEST(UptaneNetwork, no_errors) {
  EXPECT_TRUE(doInit(kFileSystem, "noerrors", "noerrors"));
}

//The same tests but with sqlite backend
TEST(UptaneNetwork, device_drop_request_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "drop_request", "noerrors"));
}

TEST(UptaneNetwork, device_drop_body_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "drop_body", "noerrors"));
}

TEST(UptaneNetwork, device_503_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "status_503", "noerrors"));
}

TEST(UptaneNetwork, device_408_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "status_408", "noerrors"));
}

TEST(UptaneNetwork, ecu_drop_request_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "noerrors", "drop_request"));
}

TEST(UptaneNetwork, ecu_503_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "noerrors", "status_503"));
}

TEST(UptaneNetwork, ecu_408_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "noerrors", "status_408"));
}

TEST(UptaneNetwork, no_connection_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "noconnection", "noerrors"));
}

TEST(UptaneNetwork, no_errors_sqlite) {
  EXPECT_TRUE(doInit(kSqlite, "noerrors", "noerrors"));
}
// clang-format on

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);

  port = TestUtils::getFreePort();
  TestHelperProcess server_process("tests/fake_http_server/fake_uptane_server.py", port);

  sleep(3);

  conf.provision.server = "http://127.0.0.1:" + port;
  conf.tls.server = "http://127.0.0.1:" + port;
  conf.uptane.repo_server = conf.tls.server + "/repo";
  conf.uptane.director_server = conf.tls.server + "/director";
  conf.pacman.ostree_server = conf.tls.server + "/treehub";

  return RUN_ALL_TESTS();
}
#endif
