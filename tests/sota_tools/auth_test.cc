#include <curl/curl.h>
#include <gtest/gtest.h>

#include <string>

#include "authenticate.h"
#include "treehub_server.h"

#include "test_utils.h"

TEST(authenticate, good_zip) {
  std::string filepath = "sota_tools/auth_test_good.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(0, r);
}

TEST(authenticate, good_cert_zip) {
  std::string filepath = "sota_tools/auth_test_cert_good.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(0, r);
  CURL *curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle);
  CURLcode rc = curl_easy_perform(curl_handle);

  EXPECT_EQ(CURLE_OK, rc);
}

TEST(authenticate, bad_cert_zip) {
  std::string filepath = "sota_tools/auth_test_cert_bad.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(0, r);
  CURL *curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle);
  CURLcode rc = curl_easy_perform(curl_handle);

  EXPECT_NE(CURLE_OK, rc);
}

TEST(authenticate, bad_zip) {
  std::string filepath = "sota_tools/auth_test_bad.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, no_json_zip) {
  std::string filepath = "sota_tools/auth_test_no_json.zip";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, good_json) {
  std::string filepath = "sota_tools/auth_test_good.json";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(0, r);
}

TEST(authenticate, bad_json) {
  std::string filepath = "sota_tools/auth_test_bad.json";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, invalid_file) {
  std::string filepath = "sota_tools/auth_test.cc";
  TreehubServer treehub;
  int r = authenticate("", filepath, treehub);
  EXPECT_EQ(1, r);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  TestHelperProcess server_process("fake_http_server/ssl_server.py", "");
  sleep(2);
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
