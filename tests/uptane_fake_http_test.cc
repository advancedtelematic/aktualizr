#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <string>

#include "fsstorage.h"
#include "logger.h"
#include "test_utils.h"
#include "uptane/uptanerepository.h"
#include "utils.h"

void doDeviceRegister(const std::string &device_id, Config &conf, bool expect) {
  system((std::string("rm -rf ") + conf.tls.certificates_directory.string() + "/*").c_str());

  FSStorage storage(conf);
  conf.uptane.device_id = device_id;
  Uptane::Repository *uptane = new Uptane::Repository(conf, storage);
  bool result = uptane->deviceRegister();
  delete uptane;

  EXPECT_EQ(result, expect);
  EXPECT_EQ(boost::filesystem::exists(conf.tls.certificates_directory / conf.tls.client_certificate), expect);
  EXPECT_EQ(boost::filesystem::exists(conf.tls.certificates_directory / conf.tls.ca_file), expect);
  EXPECT_EQ(boost::filesystem::exists(conf.tls.certificates_directory / conf.tls.pkey_file), expect);
}

void doEcuRegister(const std::string &ecu_serial, Config &conf, bool expect) {
  conf.uptane.device_id = "noerrors";
  conf.uptane.primary_ecu_serial = ecu_serial;
  FSStorage storage(conf);
  Uptane::Repository *uptane = new Uptane::Repository(conf, storage);
  std::string url_backup = conf.tls.server;

  EXPECT_EQ(uptane->deviceRegister(), true);
  if (ecu_serial == "noconnection") {
    conf.tls.server = "http://127.0.0.1:1";
    EXPECT_EQ(uptane->ecuRegister(), false);
    conf.tls.server = url_backup;
  } else {
    EXPECT_EQ(uptane->ecuRegister(), expect);
  }

  delete uptane;
}

/**
  * \verify{\tst{158}} Check that aktualizr can complete provisioning after previous network issues
*/
TEST(SotaUptaneClientTest, partial_provision) {
  std::string port = getFreePort();
  pid_t pID = fork();
  if (pID == 0) {
    execlp("tests/fake_http_server/fake_uptane_server.py", "fake_uptane_server.py", port.c_str(), (char *)0);
    return;
  }
  sleep(3);

  Config conf("tests/config_tests_prov.toml");
  conf.tls.server = "http://127.0.0.1:" + port;

  doDeviceRegister("drop_request", conf, false);
  doDeviceRegister("noerrors", conf, true);

  doDeviceRegister("drop_body", conf, false);
  doDeviceRegister("noerrors", conf, true);

  doDeviceRegister("status_503", conf, false);
  doDeviceRegister("noerrors", conf, true);

  doDeviceRegister("status_408", conf, false);
  doDeviceRegister("noerrors", conf, true);

  conf.tls.server = "http://127.0.0.1:1";
  doDeviceRegister("noerrors", conf, false);
  conf.tls.server = "http://127.0.0.1:" + port;
  doDeviceRegister("noerrors", conf, true);

  doEcuRegister("drop_request", conf, false);
  doEcuRegister("noerrors", conf, true);

  doEcuRegister("status_503", conf, false);
  doEcuRegister("noerrors", conf, true);

  doEcuRegister("status_503", conf, false);
  doEcuRegister("noerrors", conf, true);

  doEcuRegister("noconnection", conf, false);
  doEcuRegister("noerrors", conf, true);

  kill(pID, SIGTERM);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  loggerSetSeverity(LVL_trace);
  return RUN_ALL_TESTS();
}
#endif
