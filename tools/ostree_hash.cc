#include "ostree_hash.h"

#include <cstring>
#include <sstream>
#include <iomanip>

OSTreeHash::OSTreeHash(const uint8_t hash[32]) { std::memcpy(hash_, hash, 32); }

std::string OSTreeHash::string() const {
  std::stringstream str_str;
  str_str.fill('0');

  // sha256 hash is always 256 bits = 32 bytes long
  for (int i = 0; i < 32; i++)
    str_str << std::setw(2) << std::hex << (unsigned short)hash_[i];
  return str_str.str();
}

bool OSTreeHash::operator<(const OSTreeHash& other) const {
  return memcmp(hash_, other.hash_, 32) < 0;
}

std::ostream& operator<<(std::ostream& os, const OSTreeHash& obj) {
  os << obj.string();
  return os;
}