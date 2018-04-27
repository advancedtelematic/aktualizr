#include <gtest/gtest.h>

#include "ostree_hash.h"

using std::string;

TEST(ostree_hash, parse) {
  string orig = "1f3378927c2d062e40a372414c920219e506afeb8ef25f9ff72a27b792cd093a";
  EXPECT_NO_THROW(OSTreeHash::Parse(orig));
  OSTreeHash h = OSTreeHash::Parse(orig);
  EXPECT_EQ(orig, h.string());
}

TEST(ostree_hash, parse_mixed_case) {
  string orig = "1F3378927C2d062e40a372414c920219e506afeb8ef25f9ff72a27b792cd093a";
  string expected = "1f3378927c2d062e40a372414c920219e506afeb8ef25f9ff72a27b792cd093a";
  EXPECT_NO_THROW(OSTreeHash::Parse(orig));
  OSTreeHash h = OSTreeHash::Parse(orig);
  EXPECT_EQ(expected, h.string());
}

TEST(ostree_hash, parse_empty) { EXPECT_THROW(OSTreeHash::Parse(""), OSTreeCommitParseError); }

TEST(ostree_hash, parse_short) { EXPECT_THROW(OSTreeHash::Parse("1F3378927C2d062"), OSTreeCommitParseError); }

TEST(ostree_hash, parse_junk) {
  string str = "1F3378927C2d062e40a372414c920219e506afeb8ef25f9ff72a27b792cd093a";
  EXPECT_NO_THROW(OSTreeHash::Parse(str));
  str[5] = 'z';
  EXPECT_THROW(OSTreeHash::Parse(str), OSTreeCommitParseError);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
