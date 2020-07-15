#include "ostree_hash.h"

#include <cstring>  // memcmp, memcpy
#include <iomanip>
#include <sstream>

OSTreeHash OSTreeHash::Parse(const std::string& hash) {
  std::array<uint8_t, 32> sha256{};
  std::string trimmed_hash = hash.substr(0, hash.find_last_not_of(" \t\n\r\f\v") + 1);

  std::istringstream refstr(trimmed_hash);

  if (trimmed_hash.size() != 64) {
    std::cout << "HASH size: " << trimmed_hash.size() << "\n";
    throw OSTreeCommitParseError("OSTree Hash has invalid length");
  }
  // sha256 is always 256 bits == 32 bytes long
  for (size_t i = 0; i < sha256.size(); i++) {
    std::array<char, 3> byte_string{};
    byte_string[2] = 0;
    uint64_t byte_holder;

    refstr.read(byte_string.data(), 2);
    char* next_char;
    byte_holder = strtoul(byte_string.data(), &next_char, 16);
    if (next_char != &byte_string[2]) {
      throw OSTreeCommitParseError("Invalid character in OSTree commit hash");
    }
    sha256[i] = byte_holder & 0xFF;  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  }
  return OSTreeHash(sha256);
}

// NOLINTNEXTLINE(modernize-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays, hicpp-avoid-c-arrays)
OSTreeHash::OSTreeHash(const uint8_t hash[32]) { std::memcpy(hash_.data(), hash, hash_.size()); }

OSTreeHash::OSTreeHash(const std::array<uint8_t, 32>& hash) { std::memcpy(hash_.data(), hash.data(), hash.size()); }

std::string OSTreeHash::string() const {
  std::stringstream str_str;
  str_str.fill('0');

  // sha256 hash is always 256 bits = 32 bytes long
  for (size_t i = 0; i < hash_.size(); i++) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    str_str << std::setw(2) << std::hex << static_cast<uint64_t>(hash_[i]);
  }
  return str_str.str();
}

bool OSTreeHash::operator<(const OSTreeHash& other) const {
  return memcmp(hash_.data(), other.hash_.data(), hash_.size()) < 0;
}

std::ostream& operator<<(std::ostream& os, const OSTreeHash& obj) {
  os << obj.string();
  return os;
}
