#include <gtest/gtest.h>

#include "logging/logging.h"
#include "uptane/exceptions.h"
#include "uptane/tuf.h"
#include "utilities/utils.h"

TEST(TufHash, EncodeDecode) {
  std::vector<Uptane::Hash> hashes = {{Uptane::Hash::Type::kSha256, "abcd"}, {Uptane::Hash::Type::kSha512, "defg"}};

  std::string encoded = Uptane::Hash::encodeVector(hashes);
  std::vector<Uptane::Hash> decoded = Uptane::Hash::decodeVector(encoded);

  EXPECT_EQ(hashes, decoded);
}

TEST(TufHash, DecodeBad) {
  std::string bad1 = ":";
  EXPECT_EQ(Uptane::Hash::decodeVector(bad1), std::vector<Uptane::Hash>{});

  std::string bad2 = ":abcd;sha256:12";
  EXPECT_EQ(Uptane::Hash::decodeVector(bad2),
            std::vector<Uptane::Hash>{Uptane::Hash(Uptane::Hash::Type::kSha256, "12")});

  std::string bad3 = "sha256;";
  EXPECT_EQ(Uptane::Hash::decodeVector(bad3), std::vector<Uptane::Hash>{});

  std::string bad4 = "sha256:;";
  EXPECT_EQ(Uptane::Hash::decodeVector(bad4), std::vector<Uptane::Hash>{});
}

#ifndef __NO_MAIN__
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
