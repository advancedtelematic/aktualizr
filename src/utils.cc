#include "utils.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

std::string Utils::fromBase64(std::string base64_string) {
  unsigned long long paddingChars = std::count(base64_string.begin(), base64_string.end(), '=');
  std::replace(base64_string.begin(), base64_string.end(), '=', 'A');
  std::string result(base64_to_bin(base64_string.begin()), base64_to_bin(base64_string.end()));
  result.erase(result.end() - static_cast<unsigned int>(paddingChars), result.end());
  return result;
}

std::string Utils::toBase64(std::string tob64) {
  std::string b64sig(base64_text(tob64.begin()), base64_text(tob64.end()));
  b64sig.append((3 - tob64.length() % 3) % 3, '=');
  return b64sig;
}

Json::Value Utils::parseJSON(const std::string &json_str) {
  Json::Reader reader;
  Json::Value json_value;
  reader.parse(json_str, json_value);
  return json_value;
}

Json::Value Utils::parseJSONFile(const boost::filesystem::path &filename) {
  std::ifstream path_stream(filename.string().c_str());
  std::string content((std::istreambuf_iterator<char>(path_stream)), std::istreambuf_iterator<char>());
  return Utils::parseJSON(content);
}