#include <gtest/gtest.h>

#include "crypto/crypto.h"
#include "deploy.h"
#include "garage_common.h"
#include "ostree_dir_repo.h"
#include "ostree_http_repo.h"
#include "ostree_ref.h"
#include "test_utils.h"

std::string port = "2443";
TemporaryDirectory temp_dir;

/* Fetch OSTree objects from source repository and push to destination repository.
 * Parse OSTree object to identify child objects. */
TEST(deploy, UploadToTreehub) {
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeDirRepo>("tests/sota_tools/repo");
  boost::filesystem::path filepath = (temp_dir.Path() / "auth.json").string();
  boost::filesystem::path cert_path = "tests/fake_http_server/client.crt";

  const uint8_t hash[32] = {0x7f, 0x92, 0x37, 0xe1, 0x04, 0x8f, 0xa9, 0x7b, 0x20, 0x4a, 0x2c,
                            0x42, 0x02, 0x8e, 0x03, 0xee, 0xc2, 0x7d, 0x89, 0x1b, 0x5d, 0xfa,
                            0xb0, 0x58, 0x95, 0x89, 0xa3, 0x89, 0x87, 0xd7, 0x61, 0x99};
  UploadToTreehub(src_repo, ServerCredentials(filepath), OSTreeHash(hash), cert_path.string(), RunMode::kDefault, 2);

  int result = system(
      (std::string("diff -r ") + (temp_dir.Path() / "objects/").string() + " tests/sota_tools/repo/objects/").c_str());
  EXPECT_EQ(result, 0) << "Diff between source and destination repos is nonzero.";
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  port = TestUtils::getFreePort();
  std::string server = "tests/sota_tools/treehub_deploy_server.py";
  Json::Value auth;
  auth["ostree"]["server"] = std::string("https://localhost:") + port;
  Utils::writeFile(temp_dir.Path() / "auth.json", auth);

  TestHelperProcess server_process(server, port, temp_dir.PathString());
  TestUtils::waitForServer("https://localhost:" + port + "/");
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
