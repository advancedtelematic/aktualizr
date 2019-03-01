#include <gtest/gtest.h>

#include "deploy.h"
#include "garage_common.h"
#include "ostree_http_repo.h"
#include "ostree_ref.h"
#include "server_credentials.h"
#include "test_utils.h"

std::string port;

/* Verify a remote OSTree repository. */
TEST(http_repo, valid_repo) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_TRUE(src_repo->LooksValid());
}

/* Reject an invalid remote OSTree repository. */
TEST(http_repo, invalid_repo) {
  TreehubServer server;
  server.root_url("http://wronghost");
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_FALSE(src_repo->LooksValid());
}

/* Find OSTree ref in remote repository. */
TEST(http_repo, getRef) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_EQ(src_repo->GetRef("master").GetHash().string(),
            std::string("7f9237e1048fa97b204a2c42028e03eec27d891b5dfab0589589a38987d76199"));
}

/* Fetch OSTree object from remote repository.
 * Check all valid OSTree object extensions. */
TEST(http_repo, GetObject) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  const uint8_t hash[32] = {0x2a, 0x28, 0xda, 0xc4, 0x2b, 0x76, 0xc2, 0x01, 0x5e, 0xe3, 0xc4,
                            0x1c, 0xc4, 0x18, 0x3b, 0xb8, 0xb5, 0xc7, 0x90, 0xfd, 0x21, 0xfa,
                            0x5c, 0xfa, 0x08, 0x02, 0xc6, 0xe1, 0x1f, 0xd0, 0xed, 0xbe};
  auto object = src_repo->GetObject(hash, OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META);
  std::stringstream s;
  s << object;
  EXPECT_EQ(s.str(), std::string("2a/28dac42b76c2015ee3c41cc4183bb8b5c790fd21fa5cfa0802c6e11fd0edbe.dirmeta"));
}

/* Abort if OSTree object is not found after retry. */
TEST(http_repo, GetWrongObject) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  const uint8_t hash[32] = {0x00, 0x28, 0xda, 0xc4, 0x2b, 0x76, 0xc2, 0x01, 0x5e, 0xe3, 0xc4,
                            0x1c, 0xc4, 0x18, 0x3b, 0xb8, 0xb5, 0xc7, 0x90, 0xfd, 0x21, 0xfa,
                            0x5c, 0xfa, 0x08, 0x02, 0xc6, 0xe1, 0x1f, 0xd0, 0xed, 0xbe};
  EXPECT_THROW(src_repo->GetObject(hash, OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META), OSTreeObjectMissing);
}

/* Retry fetch if not found after first try.
 *
 * This test uses servers that drop every other request. The test should pass
 * anyway because we've learned not to always trust the server the first time
 * and to try again before giving up. */
TEST(http_repo, bad_connection) {
  TemporaryDirectory temp_dir;
  std::string sp = TestUtils::getFreePort();

  TestHelperProcess server_process("tests/sota_tools/treehub_server.py", sp, std::string("2"));
  TestUtils::waitForServer("http://localhost:" + sp + "/");

  TreehubServer server;
  server.root_url("http://localhost:" + sp);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);

  std::string dp = TestUtils::getFreePort();
  Json::Value auth;
  auth["ostree"]["server"] = std::string("https://localhost:") + dp;
  Utils::writeFile(temp_dir.Path() / "auth.json", auth);
  TestHelperProcess deploy_server_process("tests/sota_tools/treehub_deploy_server.py", dp, temp_dir.PathString(),
                                          std::string("2"));
  TestUtils::waitForServer("https://localhost:" + dp + "/");

  boost::filesystem::path filepath = (temp_dir.Path() / "auth.json").string();
  boost::filesystem::path cert_path = "tests/fake_http_server/client.crt";

  const uint8_t hash[32] = {0x7f, 0x92, 0x37, 0xe1, 0x04, 0x8f, 0xa9, 0x7b, 0x20, 0x4a, 0x2c,
                            0x42, 0x02, 0x8e, 0x03, 0xee, 0xc2, 0x7d, 0x89, 0x1b, 0x5d, 0xfa,
                            0xb0, 0x58, 0x95, 0x89, 0xa3, 0x89, 0x87, 0xd7, 0x61, 0x99};
  UploadToTreehub(src_repo, ServerCredentials(filepath), OSTreeHash(hash), cert_path.string(), RunMode::kDefault, 1);

  int result = system(
      (std::string("diff -r ") + (src_repo->root() / "objects/").string() + " tests/sota_tools/repo/objects/").c_str());
  EXPECT_EQ(result, 0) << "Diff between source and destination repos is nonzero.";
}

TEST(http_repo, root) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_TRUE(src_repo->LooksValid());
  std::string conf = Utils::readFile(src_repo->root() / "config");
  EXPECT_EQ(conf, std::string("[core]\nrepo_version=1\nmode=archive-z2\n"));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  std::string server = "tests/sota_tools/treehub_server.py";
  port = TestUtils::getFreePort();

  TestHelperProcess server_process(server, port);
  TestUtils::waitForServer("http://localhost:" + port + "/");

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
