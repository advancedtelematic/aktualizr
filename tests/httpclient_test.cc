#include <stdio.h>
#include <cstdlib>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config.h"
#include "httpclient.h"
#include "json/json.h"
#include "types.h"

const std::string server = "http://127.0.0.1:8800";

TEST(CopyConstructorTest, copied) {
  HttpClient* http = new HttpClient();
  HttpClient http_copy(*http);
  delete http;
  std::string path = "/path/1/2/3";
  Json::Value resp = http_copy.getJson(server + path);
  EXPECT_EQ(resp["path"].asString(), path);
}

TEST(AuthenticateTest, authenticated) {
  HttpClient http;
  AuthConfig conf;
  conf.server = "http://127.0.0.1:8800";
  conf.client_id = "id";
  conf.client_secret = "secret";
  bool response = http.authenticate(conf);
  EXPECT_EQ(response, true);
  Json::Value resp = http.getJson(server + "/auth_call");
  EXPECT_EQ(resp["status"].asString(), "good");
}

TEST(GetTest, get_performed) {
  HttpClient http;
  std::string path = "/path/1/2/3";
  Json::Value response = http.getJson(server + path);
  EXPECT_EQ(response["path"].asString(), path);
}

TEST(PostTest, post_performed) {
  HttpClient http;
  std::string path = "/path/1/2/3";
  std::string data = "{\"key\":\"val\"}";
  Json::Reader reader;
  Json::Value json;
  reader.parse(data, json);
  Json::Value response = http.post(server + path, json);
  EXPECT_EQ(response["path"].asString(), path);
  EXPECT_EQ(response["data"]["key"].asString(), "val");
}


TEST(PostTest, put_performed) {
  HttpClient http;
  std::string path = "/path/1/2/3";
  std::string data = "{\"key\":\"val\"}";

  std::string response = http.put(server + path, data);
  Json::Reader reader;
  Json::Value json;
  reader.parse(response, json);
  
  EXPECT_EQ(json["path"].asString(), path);
  EXPECT_EQ(json["data"]["key"].asString(), "val");
}


TEST(GetTest, file_downloaded) {
  HttpClient http;
  std::string path = "/download";
  std::string filename = "/tmp/aktualizr_test_http.txt";
  remove(filename.c_str());
  bool result = http.download(server + path, filename);
  EXPECT_EQ(result, true);
  std::ifstream file_stream(filename.c_str());
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ(content, "content");
}

TEST(GetTest, file_downloaded_resume) {
  HttpClient http;
  std::string path = "/download";
  std::string filename = "/tmp/aktualizr_test_http.txt";
  remove(filename.c_str());
  bool result = http.download(server + path, filename);
  EXPECT_EQ(result, true);
  result = http.download(server + path, filename);
  EXPECT_EQ(result, true);
  std::ifstream file_stream(filename.c_str());
  std::string content;
  std::getline(file_stream, content);
  EXPECT_EQ(content, "contentcontent");
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc >= 2) {
    std::string command = std::string(argv[1]) + "/fake_http_server.py &";
    EXPECT_EQ(system(command.c_str()), 0);
    sleep(1);
  }
  int ret = RUN_ALL_TESTS();
  if (argc >= 2) {
    EXPECT_EQ(system("killall fake_http_server.py"), 0);
  }
  return ret;
}
#endif
