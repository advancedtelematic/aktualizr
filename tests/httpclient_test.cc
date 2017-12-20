#include <gtest/gtest.h>

#include <errno.h>
#include <stdio.h>
#include <cstdlib>

#include "json/json.h"

#include "config.h"
#include "httpclient.h"
#include "test_utils.h"
#include "types.h"
#include "utils.h"

static std::string server = "http://127.0.0.1:";

TEST(CopyConstructorTest, copied) {
  HttpClient http;
  HttpClient http_copy(http);
  std::string path = "/path/1/2/3";
  Json::Value resp = http_copy.get(server + path).getJson();
  EXPECT_EQ(resp["path"].asString(), path);
}

TEST(GetTest, get_performed) {
  HttpClient http;
  std::string path = "/path/1/2/3";
  Json::Value response = http.get(server + path).getJson();
  EXPECT_EQ(response["path"].asString(), path);
}

TEST(PostTest, post_performed) {
  HttpClient http;
  std::string path = "/path/1/2/3";
  Json::Value data;
  data["key"] = "val";

  Json::Value response = http.post(server + path, data).getJson();
  EXPECT_EQ(response["path"].asString(), path);
  EXPECT_EQ(response["data"]["key"].asString(), "val");
}

TEST(PostTest, put_performed) {
  HttpClient http;
  std::string path = "/path/1/2/3";
  Json::Value data;
  data["key"] = "val";

  Json::Value json = http.put(server + path, data).getJson();

  EXPECT_EQ(json["path"].asString(), path);
  EXPECT_EQ(json["data"]["key"].asString(), "val");
}

// TODO: add tests for HttpClient::download

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    std::string port = TestUtils::getFreePort();
    std::cout << "port number: " << port << "\n";
    pid_t pID = fork();
    if (pID == 0) {
      execlp((std::string(argv[1]) + "/fake_http_server.py").c_str(), "fake_http_server.py", port.c_str(), (char*)0);
      return 0;
    } else {
      sleep(4);
      server += port;
      int ret = RUN_ALL_TESTS();
      kill(pID, SIGTERM);
      return ret;
    }
  }
  return -1;
}
#endif
