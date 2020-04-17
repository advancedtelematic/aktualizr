#include <gtest/gtest.h>

#include "logging/logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"
#include "utilities/utils.h"

TEST(TufHash, EncodeDecode) {
  std::vector<Hash> hashes = {{Hash::Type::kSha256, "abcd"}, {Hash::Type::kSha512, "defg"}};

  std::string encoded = Hash::encodeVector(hashes);
  std::vector<Hash> decoded = Hash::decodeVector(encoded);

  EXPECT_EQ(hashes, decoded);
}

TEST(TufHash, DecodeBad) {
  std::string bad1 = ":";
  EXPECT_EQ(Hash::decodeVector(bad1), std::vector<Hash>{});

  std::string bad2 = ":abcd;sha256:12";
  EXPECT_EQ(Hash::decodeVector(bad2), std::vector<Hash>{Hash(Hash::Type::kSha256, "12")});

  std::string bad3 = "sha256;";
  EXPECT_EQ(Hash::decodeVector(bad3), std::vector<Hash>{});

  std::string bad4 = "sha256:;";
  EXPECT_EQ(Hash::decodeVector(bad4), std::vector<Hash>{});
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
