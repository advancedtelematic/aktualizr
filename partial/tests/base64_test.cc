#include <gtest/gtest.h>

#include "libuptiny/base64.h"
#include "utilities/utils.h"
#include "logging/logging.h"

TEST(tinybase64, encode) {
  char buf[100];

  std::string mod0 = "123456789";
  base64_encode(reinterpret_cast<const uint8_t*>(mod0.c_str()), 9, buf);
  EXPECT_EQ(Utils::fromBase64(std::string(buf)), mod0);

  std::string mod2 = "12345678";
  base64_encode(reinterpret_cast<const uint8_t*>(mod2.c_str()), 8, buf);
  EXPECT_EQ(Utils::fromBase64(std::string(buf)), mod2);

  std::string mod1 = "1234567";
  base64_encode(reinterpret_cast<const uint8_t*>(mod1.c_str()), 7, buf);
  EXPECT_EQ(Utils::fromBase64(std::string(buf)), mod1);

  std::string len1 = "1";
  base64_encode(reinterpret_cast<const uint8_t*>(len1.c_str()), 1, buf);
  EXPECT_EQ(Utils::fromBase64(std::string(buf)), len1);

  std::string len2 = "12";
  base64_encode(reinterpret_cast<const uint8_t*>(len2.c_str()), 2, buf);
  EXPECT_EQ(Utils::fromBase64(std::string(buf)), len2);

  std::string len0 = "";
  base64_encode(reinterpret_cast<const uint8_t*>(len1.c_str()), 0, buf);
  EXPECT_EQ(Utils::fromBase64(std::string(buf)), len0);
}

TEST(tinybase64, decode) {
  uint8_t decoded_buf[10];

  std::string mod0 = "123456789";
  EXPECT_EQ(base64_decode(Utils::toBase64(mod0).c_str(), (unsigned int)(Utils::toBase64(mod0).length()), decoded_buf) , 9);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(decoded_buf), 9), mod0);

  std::string mod2 = "12345678";
  EXPECT_EQ(base64_decode(Utils::toBase64(mod2).c_str(), (unsigned int)(Utils::toBase64(mod2).length()), decoded_buf) , 8);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(decoded_buf), 8), mod2);

  std::string mod1 = "1234567";
  EXPECT_EQ(base64_decode(Utils::toBase64(mod1).c_str(), (unsigned int)(Utils::toBase64(mod1).length()), decoded_buf) , 7);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(decoded_buf), 7), mod1);

  std::string len1 = "1";
  EXPECT_EQ(base64_decode(Utils::toBase64(len1).c_str(), (unsigned int)(Utils::toBase64(len1).length()), decoded_buf) , 1);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(decoded_buf), 1), len1);

  std::string len2 = "12";
  EXPECT_EQ(base64_decode(Utils::toBase64(len2).c_str(), (unsigned int)(Utils::toBase64(len2).length()), decoded_buf) , 2);
  EXPECT_EQ(std::string(reinterpret_cast<char*>(decoded_buf), 2), len2);

  std::string len0 = "";
  EXPECT_EQ(base64_decode(Utils::toBase64(len0).c_str(), (unsigned int)(Utils::toBase64(len0).length()), decoded_buf), 0);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logger_init();
  logger_set_threshold(boost::log::trivial::trace);
  return RUN_ALL_TESTS();
}
#endif
