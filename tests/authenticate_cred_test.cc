#include <gtest/gtest.h>

#include <string>

#include <curl/curl.h>
#include <boost/process.hpp>

#include "authenticate.h"
#include "server_credentials.h"
#include "test_utils.h"
#include "treehub_server.h"
#include "utilities/utils.h"

boost::filesystem::path good_test_cred;
boost::filesystem::path good_auth_json;

/* Authenticate with OAuth2.
 * Parse authentication information from treehub.json. */
TEST(authenticate, good_zip) {
  // Authenticates with the ATS portal to the SaaS instance.
  ServerCredentials creds(good_test_cred);
  EXPECT_EQ(creds.GetMethod(), AuthMethod::kOauth2);
  TreehubServer treehub;
  int r = authenticate("", creds, treehub);
  EXPECT_EQ(0, r);
}

/* Extract credentials from a provided JSON file. */
TEST(authenticate, good_json) {
  // Authenticates with the ATS portal to the SaaS instance.
  // Outdated. we can probably get rid of the whole json-only authentication at this point. T
  // he last time that was officially supported was over three years ago(2017)
  // and it's been "deprecated" ever since.
  TreehubServer treehub;
  int r = authenticate("", ServerCredentials(good_auth_json), treehub);
  EXPECT_EQ(0, r);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  std::string cmd_output;
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "Error: " << argv[0] << " requires the path to the credential.zip.\n";
    return EXIT_FAILURE;
  }
  good_test_cred = argv[1];

  // prepare auth_test_good.json, use treehub.json from SOTA_PACKED_CREDENTIALS
  // extract treehub.json from and save it as auth_test_good.json.
  TemporaryDirectory tmp_data_dir;
  good_auth_json = tmp_data_dir / "auth_test_good.json";
  auto shell_cmd = std::string("unzip -p ") + argv[1] + std::string(" treehub.json >") + good_auth_json.string();
  if (Utils::shell(shell_cmd, &cmd_output) != 0) {
    return -1;
  }

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
