#include <gtest/gtest.h>

#include <errno.h>
#include <stdio.h>
#include <cstdlib>

#include "json/json.h"

#include "http/httpclient.h"
#include "test_utils.h"
#include "utilities/types.h"
#include "utilities/utils.h"

static std::string server = "http://127.0.0.1:";

TEST(CopyConstructorTest, copied) {
  HttpClient http;
  HttpClient http_copy(http);
  std::string path = "/path/1/2/3";
  Json::Value resp = http_copy.get(server + path, HttpInterface::kNoLimit).getJson();
  EXPECT_EQ(resp["path"].asString(), path);
}

TEST(GetTest, get_performed) {
  HttpClient http;
  std::string path = "/path/1/2/3";
  Json::Value response = http.get(server + path, HttpInterface::kNoLimit).getJson();
  EXPECT_EQ(response["path"].asString(), path);
}

TEST(GetTest, download_size_limit) {
  HttpClient http;
  std::string path = "/large_file";
  HttpResponse resp = http.get(server + path, 1024);
  std::cout << "RESP SIZE " << resp.body.length() << std::endl;
  EXPECT_EQ(resp.curl_code, CURLE_FILESIZE_EXCEEDED);
}

TEST(GetTest, download_speed_limit) {
  HttpClient http;
  std::string path = "/slow_file";

  http.overrideSpeedLimitParams(3, 5000);
  HttpResponse resp = http.get(server + path, HttpInterface::kNoLimit);
  EXPECT_EQ(resp.curl_code, CURLE_OPERATION_TIMEDOUT);
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

  std::string port = TestUtils::getFreePort();
  server += port;
  TestHelperProcess server_process("tests/fake_http_server/fake_http_server.py", port);

  sleep(4);

  return RUN_ALL_TESTS();
}
#endif
