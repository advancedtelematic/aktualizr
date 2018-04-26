#include <curl/curl.h>
#include <gtest/gtest.h>

#include <string>

#include "authenticate.h"
#include "server_credentials.h"
#include "treehub_server.h"

#include "test_utils.h"

TEST(authenticate, good_zip) {
  boost::filesystem::path filepath = "sota_tools/auth_test_good.zip";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(0, r);
}

TEST(authenticate, good_cert_zip) {
  boost::filesystem::path filepath = "sota_tools/auth_test_cert_good.zip";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(0, r);
  CURL *curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle);
  CURLcode rc = curl_easy_perform(curl_handle);

  EXPECT_EQ(CURLE_OK, rc);
  curl_easy_cleanup(curl_handle);
}

TEST(authenticate, good_cert_noauth_zip) {
  boost::filesystem::path filepath = "sota_tools/auth_test_noauth_good.zip";
  TreehubServer treehub;
  int r = authenticate("fake_http_server/client.crt", ServerCredentials(filepath), treehub);
  EXPECT_EQ(0, r);
  CURL *curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle);
  CURLcode rc = curl_easy_perform(curl_handle);

  EXPECT_EQ(CURLE_OK, rc);
  curl_easy_cleanup(curl_handle);
}

TEST(authenticate, bad_cert_zip) {
  boost::filesystem::path filepath = "sota_tools/auth_test_cert_bad.zip";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(0, r);
  CURL *curl_handle = curl_easy_init();
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle);
  CURLcode rc = curl_easy_perform(curl_handle);

  EXPECT_NE(CURLE_OK, rc);
  curl_easy_cleanup(curl_handle);
}

TEST(authenticate, bad_zip) {
  boost::filesystem::path filepath = "sota_tools/auth_test_bad.zip";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, no_json_zip) {
  boost::filesystem::path filepath = "sota_tools/auth_test_no_json.zip";
  EXPECT_THROW(ServerCredentials creds(filepath), BadCredentialsContent);
}

TEST(authenticate, good_json) {
  boost::filesystem::path filepath = "sota_tools/auth_test_good.json";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(0, r);
}

TEST(authenticate, bad_json) {
  boost::filesystem::path filepath = "sota_tools/auth_test_bad.json";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(1, r);
}

TEST(authenticate, invalid_file) {
  boost::filesystem::path filepath = "sota_tools/auth_test.cc";
  EXPECT_THROW(ServerCredentials creds(filepath), BadCredentialsJson);
}

TEST(authenticate, offline_sign_creds) {
  boost::filesystem::path auth_offline = "sota_tools/auth_test_good_offline.zip";
  boost::filesystem::path auth_online = "sota_tools/auth_test_cert_good.zip";

  ServerCredentials creds_offline(auth_offline);
  ServerCredentials creds_online(auth_online);
  EXPECT_TRUE(creds_offline.CanSignOffline());
  EXPECT_FALSE(creds_online.CanSignOffline());
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int result = chdir("tests");  // TODO
  (void)result;
  TestHelperProcess server_process("fake_http_server/ssl_server.py", "");
  TestHelperProcess server_noauth_process("fake_http_server/ssl_noauth_server.py", "");
  sleep(4);
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
