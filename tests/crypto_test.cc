#include <gtest/gtest.h>
#include <iostream>
#include <fstream>

#include <string>
#include "boost/algorithm/hex.hpp"

#include "crypto.h"

TEST(crypto, sha256_is_correct) {
  std::string test_str = "This is string for testing";
  std::string expected_result = "7DF106BB55506D91E48AF727CD423B169926BA99DF4BAD53AF4D80E717A1AC9F";
  std::string result = boost::algorithm::hex(Crypto::sha256digest(test_str));
  EXPECT_EQ(expected_result, result);
}


TEST(crypto, sign_verify) {
  std::ifstream path_stream("tests/test_data/public.key");
  std::string public_key((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  path_stream.close();
  std::string text = "This is text for sign";
  std::string signature = Crypto::RSAPSSSign("tests/test_data/private.key", text);
  bool signe_is_ok = Crypto::RSAPSSVerify(public_key, signature, text); 
  EXPECT_TRUE(signe_is_ok);
}

#ifndef __NO_MAIN__
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
