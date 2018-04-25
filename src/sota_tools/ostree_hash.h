#ifndef SOTA_CLIENT_TOOLS_OSTREE_HASH_H_
#define SOTA_CLIENT_TOOLS_OSTREE_HASH_H_

#include <cstdint>
#include <cstring>  // memcmp, memcpy

#include <iostream>
#include <string>
#include <utility>

class OSTreeHash {
 public:
  /**
   * Parse an OSTree hash from a string. This will normally be a root commit.
   * @throws OSTreeCommitParseError on invalid input
   */
  static OSTreeHash Parse(const std::string& hash);

  explicit OSTreeHash(const uint8_t /*hash*/[32]);

  std::string string() const;

  bool operator<(const OSTreeHash& other) const;
  friend std::ostream& operator<<(std::ostream& os, const OSTreeHash& obj);

 private:
  uint8_t hash_[32]{};
};

class OSTreeCommitParseError : std::exception {
 public:
  explicit OSTreeCommitParseError(std::string bad_hash) : bad_hash_(std::move(bad_hash)) {}

  const char* what() const noexcept override { return "Could not parse OSTree commit"; }

  std::string bad_hash() const { return bad_hash_; }

 private:
  std::string bad_hash_;
};

#endif  // SOTA_CLIENT_TOOLS_OSTREE_HASH_H_
