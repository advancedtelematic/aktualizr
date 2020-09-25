#include <gtest/gtest.h>

#include <boost/process.hpp>

#include "get.h"
#include "test_utils.h"

static std::string server = "http://localhost:";

TEST(aktualizr_get, good) {
  Config config;
  TemporaryDirectory dir;
  config.storage.path = dir.Path();

  std::vector<std::string> headers;
  std::string body = aktualizrGet(config, server + "/path/1/2/3", headers);
  EXPECT_EQ("{\"path\": \"/path/1/2/3\"}", body);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  std::string port = TestUtils::getFreePort();
  server += port;
  boost::process::child server_process("tests/fake_http_server/fake_test_server.py", port);
  TestUtils::waitForServer(server + "/");
  return RUN_ALL_TESTS();
}
#endif
