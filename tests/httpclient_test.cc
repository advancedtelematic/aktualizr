#include <stdio.h>
#include <cstdlib>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config.h"
#include "httpclient.h"
#include "json/json.h"
#include "types.h"
#include "utils.h"

const std::string server = "http://127.0.0.1:8800";

TEST(CopyConstructorTest, copied) {
  HttpClient* http = new HttpClient();
  HttpClient http_copy(*http);
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
    std::string command = std::string(argv[1]) + "/fake_http_server.py &";
    EXPECT_EQ(system(command.c_str()), 0);
    sleep(4);
  }
  int ret = RUN_ALL_TESTS();
  if (argc >= 2) {
    EXPECT_EQ(system("killall fake_http_server.py"), 0);
  }
  return ret;
}
#endif
