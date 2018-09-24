/**
 * \file
 *
 * Check that aktualizr can complete provisioning after encountering various
 * network issues.
 */
#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "http/httpclient.h"
#include "logging/logging.h"
#include "primary/initializer.h"
#include "storage/invstorage.h"
#include "test_utils.h"
#include "uptane/uptanerepository.h"
#include "utilities/utils.h"

Config conf("tests/config/basic.toml");
std::string port;

bool doTestInit(StorageType storage_type, const std::string &device_register_state,
                const std::string &ecu_register_state) {
  LOG_INFO << "First attempt to initialize.";
  TemporaryDirectory temp_dir;
  conf.storage.type = storage_type;
  conf.storage.path = temp_dir.Path();
  conf.provision.expiry_days = device_register_state;
  conf.provision.primary_ecu_serial = ecu_register_state;
  const std::string good_url = conf.provision.server;
  if (device_register_state == "noconnection") {
    conf.provision.server = conf.provision.server.substr(conf.provision.server.size() - 2) + "11";
  }

  bool result;
  auto http = std::make_shared<HttpClient>();
  auto store = INvStorage::newStorage(conf.storage);
  {
    KeyManager keys(store, conf.keymanagerConfig());
    Initializer initializer(conf.provision, store, http, keys, {});
    result = initializer.isSuccessful();
  }
  if (device_register_state != "noerrors" || ecu_register_state != "noerrors") {
    EXPECT_FALSE(result);
    LOG_INFO << "Second attempt to initialize.";
    conf.provision.server = good_url;
    conf.provision.expiry_days = "noerrors";
    conf.provision.primary_ecu_serial = "noerrors";

    if (device_register_state == "noerrors" && ecu_register_state != "noerrors") {
      // restore a "good" ecu serial in the ecu register fault injection case
      // (the bad value has been cached in storage)
      EcuSerials serials;
      store->loadEcuSerials(&serials);
      serials[0].first = Uptane::EcuSerial(conf.provision.primary_ecu_serial);
      store->storeEcuSerials(serials);
    }

    KeyManager keys(store, conf.keymanagerConfig());
    Initializer initializer(conf.provision, store, http, keys, {});
    result = initializer.isSuccessful();
  }

  return result;
}

TEST(UptaneNetwork, device_drop_request_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "drop_request", "noerrors"));
}

TEST(UptaneNetwork, device_drop_body_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "drop_body", "noerrors"));
}

TEST(UptaneNetwork, device_503_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "status_503", "noerrors"));
}

TEST(UptaneNetwork, device_408_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "status_408", "noerrors"));
}

TEST(UptaneNetwork, ecu_drop_request_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "noerrors", "drop_request"));
}

TEST(UptaneNetwork, ecu_503_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "noerrors", "status_503"));
}

TEST(UptaneNetwork, ecu_408_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "noerrors", "status_408"));
}

TEST(UptaneNetwork, no_connection_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "noconnection", "noerrors"));
}

TEST(UptaneNetwork, no_errors_sqlite) {
  RecordProperty("zephyr_key", "OTA-991,TST-158");
  EXPECT_TRUE(doTestInit(StorageType::kSqlite, "noerrors", "noerrors"));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);

  port = TestUtils::getFreePort();
  TestHelperProcess server_process("tests/fake_http_server/fake_uptane_server.py", port);

  sleep(3);

  conf.provision.server = "http://127.0.0.1:" + port;
  conf.tls.server = "http://127.0.0.1:" + port;
  conf.provision.ecu_registration_endpoint = conf.tls.server + "/director/ecus";
  conf.uptane.repo_server = conf.tls.server + "/repo";
  conf.uptane.director_server = conf.tls.server + "/director";
  conf.pacman.ostree_server = conf.tls.server + "/treehub";

  return RUN_ALL_TESTS();
}
#endif
