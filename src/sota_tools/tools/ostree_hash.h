#ifndef SOTA_CLIENT_TOOLS_OSTREE_HASH_H_
#define SOTA_CLIENT_TOOLS_OSTREE_HASH_H_

#include <cstdint>
#include <cstring>  // memcmp, memcpy

#include <iostream>
#include <string>

class OSTreeHash {
 public:
  explicit OSTreeHash(const uint8_t[32]);

  std::string string() const;

  bool operator<(const OSTreeHash& other) const;
  friend std::ostream& operator<<(std::ostream& os, const OSTreeHash& hash);

 private:
  uint8_t hash_[32];
};

#endif  // SOTA_CLIENT_TOOLS_OSTREE_HASH_H_
