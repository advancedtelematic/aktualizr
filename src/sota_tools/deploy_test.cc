#include <gtest/gtest.h>

#include <boost/process.hpp>
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

  const uint8_t hash[32] = {0x16, 0xef, 0x2f, 0x26, 0x29, 0xdc, 0x92, 0x63, 0xfd, 0xf3, 0xc0,
                            0xf0, 0x32, 0x56, 0x3a, 0x2d, 0x75, 0x76, 0x23, 0xbb, 0xc1, 0x1c,
                            0xf9, 0x9d, 0xf2, 0x5c, 0x3c, 0x3f, 0x25, 0x8d, 0xcc, 0xbe};
  UploadToTreehub(src_repo, ServerCredentials(filepath), OSTreeHash(hash), cert_path.string(), RunMode::kDefault, 2);

  int result = system(
      (std::string("diff -r ") + (temp_dir.Path() / "objects/").string() + " tests/sota_tools/repo/objects/").c_str());
  EXPECT_EQ(result, 0) << "Diff between source and destination repos is nonzero.";
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  port = TestUtils::getFreePort();
  std::string server = "tests/sota_tools/treehub_server.py";
  Json::Value auth;
  auth["ostree"]["server"] = std::string("https://localhost:") + port;
  Utils::writeFile(temp_dir.Path() / "auth.json", auth);

  boost::process::child server_process(server, std::string("-p"), port, std::string("-d"), temp_dir.PathString(),
                                       std::string("--tls"));
  TestUtils::waitForServer("https://localhost:" + port + "/");
  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
