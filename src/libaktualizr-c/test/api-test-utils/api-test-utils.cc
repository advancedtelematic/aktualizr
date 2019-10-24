#include "api-test-utils.h"

#include <boost/process.hpp>
#include "test_utils.h"
#include "config/config.h"

std::string serverAddress;

FakeHttpServer *Run_fake_http_server(const char *serverPath, const char *metaPath) {
  std::string port = TestUtils::getFreePort();
  serverAddress = "http://127.0.0.1:" + port;

  auto *server_handle = new boost::process::child(serverPath, port, "-f", "-m", metaPath);
  TestUtils::waitForServer(serverAddress + "/");

  return server_handle;
}

void Stop_fake_http_server(FakeHttpServer *server) { delete server; }

Config *Get_test_config(const char *storagePath) {
  auto *config = new Config();

  config->tls.server = serverAddress;

  config->provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config->provision.primary_ecu_hardware_id = "primary_hw";
  config->provision.server = serverAddress;
  config->provision.provision_path = "tests/test_data/cred.zip";

  config->storage.path = storagePath;  //"/home/mcchekhovoi/Work/connect";
  config->pacman.type = PackageManager::kNone;

  config->postUpdateValues();
  return config;
}

void Remove_test_config(Config *config) { delete config; }
