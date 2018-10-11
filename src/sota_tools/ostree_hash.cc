#include "ostree_hash.h"

#include <cstring>  // memcmp, memcpy
#include <iomanip>
#include <sstream>

OSTreeHash OSTreeHash::Parse(const std::string& hash) {
  uint8_t sha256[32];
  std::string trimmed_hash = hash.substr(0, hash.find_last_not_of(" \t\n\r\f\v") + 1);
  
  std::istringstream refstr(trimmed_hash);

  if (trimmed_hash.size() != 64) {
    std::cout << "HASH size: " << trimmed_hash.size() << "\n";
    throw OSTreeCommitParseError("OSTree Hash has invalid length");
  }
  // sha256 is always 256 bits == 32 bytes long
  for (int i = 0; i < 32; i++) {
    char byte_string[3];
    byte_string[2] = 0;
    uint64_t byte_holder;

    refstr.read(byte_string, 2);
    char* next_char;
    byte_holder = strtoul(byte_string, &next_char, 16);
    if (next_char != &byte_string[2]) {
      throw OSTreeCommitParseError("Invalid character in OSTree commit hash");
    }
    sha256[i] = byte_holder & 0xFF;
  }
  return OSTreeHash(sha256);
}

OSTreeHash::OSTreeHash(const uint8_t hash[32]) { std::memcpy(hash_, hash, 32); }

std::string OSTreeHash::string() const {
  std::stringstream str_str;
  str_str.fill('0');

  // sha256 hash is always 256 bits = 32 bytes long
  for (int i = 0; i < 32; i++) {
    str_str << std::setw(2) << std::hex << static_cast<uint64_t>(hash_[i]);
  }
  return str_str.str();
}

bool OSTreeHash::operator<(const OSTreeHash& other) const { return memcmp(hash_, other.hash_, 32) < 0; }

std::ostream& operator<<(std::ostream& os, const OSTreeHash& obj) {
  os << obj.string();
  return os;
}
