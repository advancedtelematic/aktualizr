#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rsa.h>
#include <sodium.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <string>
#include <utility>

#include <libaktualizr/types.h>
#include <libaktualizr/utils.h>

// some older versions of openssl have BIO_new_mem_buf defined with fisrt parameter of type (void*)
//   which is not true and breaks our build
#undef BIO_new_mem_buf
BIO *BIO_new_mem_buf(const void *, int);

class PublicKey {
 public:
  PublicKey() = default;
  explicit PublicKey(const boost::filesystem::path &path);

  explicit PublicKey(Json::Value uptane_json);

  PublicKey(const std::string &value, KeyType type);

  std::string Value() const { return value_; }

  KeyType Type() const { return type_; }
  /**
   * Verify a signature using this public key
   */
  bool VerifySignature(const std::string &signature, const std::string &message) const;
  /**
   * Uptane Json representation of this public key.  Used in root.json
   * and during provisioning.
   */
  Json::Value ToUptane() const;

  std::string KeyId() const;
  bool operator==(const PublicKey &rhs) const;

  bool operator!=(const PublicKey &rhs) const { return !(*this == rhs); }

 private:
  // std::string can be implicitly converted to a Json::Value. Make sure that
  // the Json::Value constructor is not called accidentally.
  PublicKey(std::string);
  std::string value_;
  KeyType type_{KeyType::kUnknown};
};

/**
 * The hash of a file or Uptane metadata.  File hashes/checksums in Uptane include the length of the object, in order to
 * defeat infinite download attacks.
 */
class Hash {
 public:
  // order corresponds algorithm priority
  enum class Type { kSha256, kSha512, kUnknownAlgorithm };

  static Hash generate(Type type, const std::string &data);
  Hash(const std::string &type, const std::string &hash);
  Hash(Type type, const std::string &hash);

  bool HaveAlgorithm() const { return type_ != Type::kUnknownAlgorithm; }
  bool operator==(const Hash &other) const;
  bool operator!=(const Hash &other) const { return !operator==(other); }
  static std::string TypeString(Type type);
  std::string TypeString() const;
  Type type() const;
  std::string HashString() const { return hash_; }
  friend std::ostream &operator<<(std::ostream &os, const Hash &h);

  static std::string encodeVector(const std::vector<Hash> &hashes);
  static std::vector<Hash> decodeVector(std::string hashes_str);

 private:
  Type type_;
  std::string hash_;
};

std::ostream &operator<<(std::ostream &os, const Hash &h);

class MultiPartHasher {
 public:
  using Ptr = std::shared_ptr<MultiPartHasher>;
  static Ptr create(Hash::Type hash_type);

  virtual void update(const unsigned char *part, uint64_t size) = 0;
  virtual void reset() = 0;
  virtual std::string getHexDigest() = 0;
  virtual Hash getHash() = 0;
  virtual ~MultiPartHasher() = default;
};

class MultiPartSHA512Hasher : public MultiPartHasher {
 public:
  MultiPartSHA512Hasher() { crypto_hash_sha512_init(&state_); }
  ~MultiPartSHA512Hasher() override = default;
  void update(const unsigned char *part, uint64_t size) override { crypto_hash_sha512_update(&state_, part, size); }
  void reset() override { crypto_hash_sha512_init(&state_); }
  std::string getHexDigest() override {
    std::array<unsigned char, crypto_hash_sha512_BYTES> sha512_hash{};
    crypto_hash_sha512_final(&state_, sha512_hash.data());
    return boost::algorithm::hex(std::string(reinterpret_cast<char *>(sha512_hash.data()), crypto_hash_sha512_BYTES));
  }

  Hash getHash() override { return Hash(Hash::Type::kSha512, getHexDigest()); }

 private:
  crypto_hash_sha512_state state_{};
};

class MultiPartSHA256Hasher : public MultiPartHasher {
 public:
  MultiPartSHA256Hasher() { crypto_hash_sha256_init(&state_); }
  ~MultiPartSHA256Hasher() override = default;
  void update(const unsigned char *part, uint64_t size) override { crypto_hash_sha256_update(&state_, part, size); }
  void reset() override { crypto_hash_sha256_init(&state_); }
  std::string getHexDigest() override {
    std::array<unsigned char, crypto_hash_sha256_BYTES> sha256_hash{};
    crypto_hash_sha256_final(&state_, sha256_hash.data());
    return boost::algorithm::hex(std::string(reinterpret_cast<char *>(sha256_hash.data()), crypto_hash_sha256_BYTES));
  }

  Hash getHash() override { return Hash(Hash::Type::kSha256, getHexDigest()); }

 private:
  crypto_hash_sha256_state state_{};
};

class Crypto {
 public:
  static std::string sha256digest(const std::string &text);
  static std::string sha512digest(const std::string &text);
  static std::string RSAPSSSign(ENGINE *engine, const std::string &private_key, const std::string &message);
  static std::string Sign(KeyType key_type, ENGINE *engine, const std::string &private_key, const std::string &message);
  static std::string ED25519Sign(const std::string &private_key, const std::string &message);
  static bool parseP12(BIO *p12_bio, const std::string &p12_password, std::string *out_pkey, std::string *out_cert,
                       std::string *out_ca);
  static bool extractSubjectCN(const std::string &cert, std::string *cn);
  static StructGuard<EVP_PKEY> generateRSAKeyPairEVP(KeyType key_type);
  static bool generateRSAKeyPair(KeyType key_type, std::string *public_key, std::string *private_key);
  static bool generateEDKeyPair(std::string *public_key, std::string *private_key);
  static bool generateKeyPair(KeyType key_type, std::string *public_key, std::string *private_key);

  static bool RSAPSSVerify(const std::string &public_key, const std::string &signature, const std::string &message);
  static bool ED25519Verify(const std::string &public_key, const std::string &signature, const std::string &message);

  static bool IsRsaKeyType(KeyType type);
  static KeyType IdentifyRSAKeyType(const std::string &public_key_pem);
};

#endif  // CRYPTO_H_
