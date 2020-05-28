#ifndef SOTA_CLIENT_TOOLS_OSTREE_HASH_H_
#define SOTA_CLIENT_TOOLS_OSTREE_HASH_H_

#include <array>
#include <cstdint>
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

  explicit OSTreeHash(const uint8_t hash[32]);  // NOLINT(modernize-avoid-c-arrays)
  explicit OSTreeHash(const std::array<uint8_t, 32>& hash);

  std::string string() const;

  bool operator<(const OSTreeHash& other) const;
  friend std::ostream& operator<<(std::ostream& os, const OSTreeHash& obj);

 private:
  std::array<uint8_t, 32> hash_{};
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
