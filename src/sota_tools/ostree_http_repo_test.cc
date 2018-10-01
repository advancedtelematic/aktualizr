#include <gtest/gtest.h>

#include "ostree_http_repo.h"
#include "ostree_ref.h"
#include "test_utils.h"
std::string port;

TEST(http_repo, valid_repo) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_TRUE(src_repo->LooksValid());
}

TEST(http_repo, invalidvalid_repo) {
  TreehubServer server;
  server.root_url("http://wronghost");
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_FALSE(src_repo->LooksValid());
}

TEST(http_repo, getRef) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_EQ(src_repo->GetRef("master").GetHash().string(),
            std::string("16ef2f2629dc9263fdf3c0f032563a2d757623bbc11cf99df25c3c3f258dccbe"));
}

TEST(http_repo, GetObject) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  const uint8_t hash[32] = {0x2a, 0x28, 0xda, 0xc4, 0x2b, 0x76, 0xc2, 0x01, 0x5e, 0xe3, 0xc4,
                            0x1c, 0xc4, 0x18, 0x3b, 0xb8, 0xb5, 0xc7, 0x90, 0xfd, 0x21, 0xfa,
                            0x5c, 0xfa, 0x08, 0x02, 0xc6, 0xe1, 0x1f, 0xd0, 0xed, 0xbe};
  auto object = src_repo->GetObject(hash);
  std::stringstream s;
  s << object;
  EXPECT_EQ(s.str(), std::string("2a/28dac42b76c2015ee3c41cc4183bb8b5c790fd21fa5cfa0802c6e11fd0edbe.filez"));
}

TEST(http_repo, GetWrongObject) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  const uint8_t hash[32] = {0x00, 0x28, 0xda, 0xc4, 0x2b, 0x76, 0xc2, 0x01, 0x5e, 0xe3, 0xc4,
                            0x1c, 0xc4, 0x18, 0x3b, 0xb8, 0xb5, 0xc7, 0x90, 0xfd, 0x21, 0xfa,
                            0x5c, 0xfa, 0x08, 0x02, 0xc6, 0xe1, 0x1f, 0xd0, 0xed, 0xbe};
  EXPECT_THROW(src_repo->GetObject(hash), OSTreeObjectMissing);
}

TEST(http_repo, root) {
  TreehubServer server;
  server.root_url("http://localhost:" + port);
  OSTreeRepo::ptr src_repo = std::make_shared<OSTreeHttpRepo>(&server);
  EXPECT_TRUE(src_repo->LooksValid());
  std::string conf = Utils::readFile(src_repo->root() / "config");
  EXPECT_EQ(conf, std::string("[core]\nrepo_version=1\nmode=archive-z2"));
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  std::string server = "tests/sota_tools/treehub_server.py";
  port = TestUtils::getFreePort();

  TestHelperProcess server_process(server, port);
  sleep(3);

  return RUN_ALL_TESTS();
}
#endif

// vim: set tabstop=2 shiftwidth=2 expandtab:
