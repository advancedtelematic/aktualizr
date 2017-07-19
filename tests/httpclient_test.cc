#include <stdio.h>
#include <cstdlib>

#include <errno.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h> /* See NOTES */
#include <sys/un.h>

#include "config.h"
#include "httpclient.h"
#include "json/json.h"
#include "types.h"
#include "utils.h"

static std::string server = "http://127.0.0.1:";

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
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
      std::cout << "socket() failed: " << errno;
      return -1;
    }
    struct sockaddr_in soc_addr;
    memset(&soc_addr, 0, sizeof(struct sockaddr_in));
    soc_addr.sin_family = AF_INET;
    soc_addr.sin_addr.s_addr = INADDR_ANY;
    soc_addr.sin_port = htons(INADDR_ANY);

    if (bind(s, (struct sockaddr*)&soc_addr, sizeof(soc_addr)) == -1) {
      std::cout << "bind() failed: " << errno;
      return -1;
    }

    struct sockaddr_in sa;
    unsigned int sa_len = sizeof(sa);
    if (getsockname(s, (struct sockaddr*)&sa, &sa_len) == -1) {
      std::cout << "getsockname() failed\n";
      return -1;
    }

    std::string port = Utils::intToString(ntohs(sa.sin_port));
    std::cout << "port number: " << port << "\n";
    close(s);
    pid_t pID = fork();
    if (pID == 0) {
      execlp((std::string(argv[1]) + "/fake_http_server.py").c_str(), "fake_http_server.py", port.c_str(), (char*)0);
      return 0;
    } else {
      sleep(4);
      server += port;
      int ret = RUN_ALL_TESTS();
      int killReturn = kill(pID, SIGTERM);
      return ret;
    }
  }
  return -1;
}
#endif
