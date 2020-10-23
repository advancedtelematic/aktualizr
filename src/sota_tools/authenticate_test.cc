#include <gtest/gtest.h>

#include <string>

#include <curl/curl.h>
#include <boost/process.hpp>

#include "authenticate.h"
#include "server_credentials.h"
#include "test_utils.h"
#include "treehub_server.h"
#include "utilities/utils.h"

boost::filesystem::path certs_dir;

/* Authenticate with OAuth2.
 * Parse authentication information from treehub.json. */
TEST(authenticate, good_zip) {
  // Authenticates with the ATS portal to the SaaS instance.
  // It is outdated test. kepp it for backward compatibility
  boost::filesystem::path filepath = "tests/sota_tools/auth_test_good.zip";
  ServerCredentials creds(filepath);
  EXPECT_EQ(creds.GetMethod(), AuthMethod::kOauth2);
  TreehubServer treehub;
  int r = authenticate("", creds, treehub);
  EXPECT_EQ(0, r);
}

/* Authenticate with TLS credentials.
 * Parse Image repository URL from a provided archive. */
TEST(authenticate, good_cert_zip) {
  // Authenticates with tls_server on port 1443.
  boost::filesystem::path filepath = certs_dir / "good.zip";
  boost::filesystem::path capath = certs_dir / "server.crt";
  ServerCredentials creds(filepath);
  EXPECT_EQ(creds.GetMethod(), AuthMethod::kTls);
  TreehubServer treehub;
  int r = authenticate(capath.string(), creds, treehub);
  EXPECT_EQ(0, r);
  CurlEasyWrapper curl_handle;
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle.get());
  CURLcode rc = curl_easy_perform(curl_handle.get());
  EXPECT_EQ(CURLE_OK, rc);
}

/* Authenticate with nothing (no auth).
 * Parse authentication information from treehub.json.
 * Parse Image repository URL from a provided archive. */
TEST(authenticate, good_cert_noauth_zip) {
  // Authenticates with tls_noauth_server on port 2443.
  boost::filesystem::path filepath = "tests/sota_tools/auth_test_noauth_good.zip";
  boost::filesystem::path capath = certs_dir / "server.crt";
  ServerCredentials creds(filepath);
  EXPECT_EQ(creds.GetMethod(), AuthMethod::kNone);
  TreehubServer treehub;
  int r = authenticate(capath.string(), creds, treehub);
  EXPECT_EQ(0, r);
  CurlEasyWrapper curl_handle;
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle.get());
  CURLcode rc = curl_easy_perform(curl_handle.get());

  EXPECT_EQ(CURLE_OK, rc);
}

TEST(authenticate, bad_cert_zip) {
  // Tries to authenticates with tls_server on port 1443.
  // Fails because the intermediate cert that signed the client cert was signed
  // by a different root cert.
  boost::filesystem::path filepath = certs_dir / "bad.zip";
  ServerCredentials creds(filepath);
  EXPECT_EQ(creds.GetMethod(), AuthMethod::kTls);
  TreehubServer treehub;
  int r = authenticate("", creds, treehub);
  EXPECT_EQ(0, r);
  CurlEasyWrapper curl_handle;
  curlEasySetoptWrapper(curl_handle.get(), CURLOPT_VERBOSE, 1);
  treehub.InjectIntoCurl("test.txt", curl_handle.get());
  CURLcode rc = curl_easy_perform(curl_handle.get());

  EXPECT_NE(CURLE_OK, rc);
}

/* Reject a provided archive file with bogus credentials. */
TEST(authenticate, bad_zip) {
  boost::filesystem::path filepath = "tests/sota_tools/auth_test_bad.zip";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(1, r);
}

/* Reject a provided archive file without a treehub.json. */
TEST(authenticate, no_json_zip) {
  boost::filesystem::path filepath = "tests/sota_tools/auth_test_no_json.zip";
  EXPECT_THROW(ServerCredentials creds(filepath), BadCredentialsContent);
}

/* Extract credentials from a provided JSON file. */
TEST(authenticate, good_json) {
  // Authenticates with the ATS portal to the SaaS instance.
  // Outdated. we can probably get rid of the whole json-only authentication at this point. T
  // he last time that was officially supported was over three years ago(2017)
  // and it's been "deprecated" ever since.
  boost::filesystem::path filepath = "tests/sota_tools/auth_test_good.json";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(0, r);
}

TEST(authenticate, good_json_v2) {
  // Authenticates with new backend.
  // Note: update auth_test_good_v2.json after deploy on prod. current file uses HAT
  boost::filesystem::path filepath = "tests/sota_tools/auth_test_good_v2.json";
  TreehubServer treehub;
  // Note: enable it in https://saeljira.it.here.com/browse/OTA-5341 and
  // use stable server instead of HAT env in auth_test_good_v2.json
  int r = 0;  // authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(0, r);
}

/* Reject a bogus provided JSON file. */
TEST(authenticate, bad_json) {
  boost::filesystem::path filepath = "tests/sota_tools/auth_test_bad.json";
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(filepath), treehub);
  EXPECT_EQ(1, r);
}

/* Reject a bogus provided file. */
TEST(authenticate, invalid_file) {
  boost::filesystem::path filepath = "tests/sota_tools/auth_test.cc";
  EXPECT_THROW(ServerCredentials creds(filepath), BadCredentialsJson);
}

/* Check if credentials support offline signing. */
TEST(authenticate, offline_sign_creds) {
  // Note that these credentials point to the old CI infrastructure that is now
  // defunct. However, for the sake of this test, that doesn't matter.
  boost::filesystem::path auth_offline = "tests/sota_tools/auth_test_good_offline.zip";
  ServerCredentials creds_offline(auth_offline);
  EXPECT_TRUE(creds_offline.CanSignOffline());
}

/* Check if credentials do not support offline signing. */
TEST(authenticate, online_sign_creds) {
  // Authenticates with tls_server on port 1443.
  boost::filesystem::path auth_online = certs_dir / "good.zip";
  ServerCredentials creds_online(auth_online);
  EXPECT_FALSE(creds_online.CanSignOffline());
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the directory with generated certificates.\n";
    return EXIT_FAILURE;
  }
  certs_dir = argv[1];

  boost::process::child server_process("tests/sota_tools/authentication/tls_server.py", "1443", certs_dir);
  boost::process::child server_noauth_process("tests/sota_tools/authentication/tls_server.py", "--noauth", "2443",
                                              certs_dir);
  // TODO: this do not work because the server expects auth! Let's sleep for now.
  // (could be replaced by a check with raw tcp)
  // TestUtils::waitForServer("https://localhost:1443/");
  sleep(4);
  TestUtils::waitForServer("https://localhost:2443/");
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
