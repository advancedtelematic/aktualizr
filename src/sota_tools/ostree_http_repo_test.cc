#include <gtest/gtest.h>

#include <boost/process.hpp>

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
            std::string("b9ac1e45f9227df8ee191b6e51e09417bd36c6ebbeff999431e3073ac50f0563"));
}

/* Fetch OSTree object from remote repository.
 * Check all valid OSTree object extensions. */
TEST(http_repo, GetObject) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  const uint8_t hash[32] = {0x44, 0x6a, 0x0e, 0xf1, 0x1b, 0x7c, 0xc1, 0x67, 0xf3, 0xb6, 0x03,
                            0xe5, 0x85, 0xc7, 0xee, 0xee, 0xb6, 0x75, 0xfa, 0xa4, 0x12, 0xd5,
                            0xec, 0x73, 0xf6, 0x29, 0x88, 0xeb, 0x0b, 0x6c, 0x54, 0x88};
  auto object = src_repo->GetObject(hash, OstreeObjectType::OSTREE_OBJECT_TYPE_DIR_META);
  std::stringstream s;
  s << object;
  EXPECT_EQ(s.str(), std::string("44/6a0ef11b7cc167f3b603e585c7eeeeb675faa412d5ec73f62988eb0b6c5488.dirmeta"));
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
  TemporaryDirectory src_dir, dst_dir;
  std::string sp = TestUtils::getFreePort();

  boost::process::child server_process("tests/sota_tools/treehub_server.py", std::string("-p"), sp, std::string("-d"),
                                       src_dir.PathString(), std::string("-f2"), std::string("--create"));
  TestUtils::waitForServer("http://localhost:" + sp + "/");

  TreehubServer server;
  server.root_url("http://localhost:" + sp);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);

  std::string dp = TestUtils::getFreePort();
  Json::Value auth;
  auth["ostree"]["server"] = std::string("https://localhost:") + dp;
  Utils::writeFile(dst_dir.Path() / "auth.json", auth);
  boost::process::child deploy_server_process("tests/sota_tools/treehub_server.py", std::string("-p"), dp,
                                              std::string("-d"), dst_dir.PathString(), std::string("-f2"),
                                              std::string("--tls"));
  TestUtils::waitForServer("https://localhost:" + dp + "/");

  boost::filesystem::path filepath = (dst_dir.Path() / "auth.json").string();
  boost::filesystem::path cert_path = "tests/fake_http_server/client.crt";

  auto hash = OSTreeHash::Parse("b9ac1e45f9227df8ee191b6e51e09417bd36c6ebbeff999431e3073ac50f0563");
  UploadToTreehub(src_repo, ServerCredentials(filepath), hash, cert_path.string(), RunMode::kDefault, 1);

  std::string diff("diff -r ");
  std::string src_path((src_dir.Path() / "objects").string() + " ");
  std::string repo_path((src_repo->root() / "objects").string() + " ");
  std::string dst_path((dst_dir.Path() / "objects").string() + " ");

  int result = system((diff + src_path + repo_path).c_str());
  result |= system((diff + repo_path + dst_path).c_str());

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

  boost::process::child server_process(server, std::string("-p"), port, std::string("--create"));
  TestUtils::waitForServer("http://localhost:" + port + "/");

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
