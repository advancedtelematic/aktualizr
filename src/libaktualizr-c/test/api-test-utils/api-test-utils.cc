#include "api-test-utils.h"

#include <boost/process.hpp>
#include "config/config.h"
#include "test_utils.h"
#include "utilities/utils.h"

std::string serverAddress;
std::unique_ptr<boost::process::child> server;
std::unique_ptr<TemporaryDirectory> temp_dir;

void Run_fake_http_server(const char *serverPath, const char *metaPath) {
  std::string port = TestUtils::getFreePort();
  serverAddress = "http://127.0.0.1:" + port;

  // NOLINTNEXTLINE(clang-analyzer-core.NonNullParamChecker)
  server = std_::make_unique<boost::process::child>(serverPath, port, "-f", "-m", metaPath);
  TestUtils::waitForServer(serverAddress + "/");
}

// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
void Stop_fake_http_server() { server.reset(); }

Config *Get_test_config() {
  auto *config = new Config();

  config->tls.server = serverAddress;

  config->provision.primary_ecu_serial = "CA:FE:A6:D2:84:9D";
  config->provision.primary_ecu_hardware_id = "primary_hw";
  config->provision.server = serverAddress;
  config->provision.provision_path = "tests/test_data/cred.zip";

  temp_dir = std_::make_unique<TemporaryDirectory>();
  config->storage.path = temp_dir->Path();
  config->pacman.type = PACKAGE_MANAGER_NONE;

  config->postUpdateValues();
  return config;
}

void Remove_test_config(Config *config) { delete config; }
