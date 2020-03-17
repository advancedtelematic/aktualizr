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

#include "utilities/types.h"
#include "utilities/utils.h"

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

class MultiPartHasher {
 public:
  virtual void update(const unsigned char *part, uint64_t size) = 0;
  virtual std::string getHexDigest() = 0;
  virtual ~MultiPartHasher() = default;
};

class MultiPartSHA512Hasher : public MultiPartHasher {
 public:
  MultiPartSHA512Hasher() { crypto_hash_sha512_init(&state_); }
  ~MultiPartSHA512Hasher() override = default;
  void update(const unsigned char *part, uint64_t size) override { crypto_hash_sha512_update(&state_, part, size); }
  std::string getHexDigest() override {
    std::array<unsigned char, crypto_hash_sha512_BYTES> sha512_hash{};
    crypto_hash_sha512_final(&state_, sha512_hash.data());
    return boost::algorithm::hex(std::string(reinterpret_cast<char *>(sha512_hash.data()), crypto_hash_sha512_BYTES));
  }

 private:
  crypto_hash_sha512_state state_{};
};

class MultiPartSHA256Hasher : public MultiPartHasher {
 public:
  MultiPartSHA256Hasher() { crypto_hash_sha256_init(&state_); }
  ~MultiPartSHA256Hasher() override = default;
  void update(const unsigned char *part, uint64_t size) override { crypto_hash_sha256_update(&state_, part, size); }
  std::string getHexDigest() override {
    std::array<unsigned char, crypto_hash_sha256_BYTES> sha256_hash{};
    crypto_hash_sha256_final(&state_, sha256_hash.data());
    return boost::algorithm::hex(std::string(reinterpret_cast<char *>(sha256_hash.data()), crypto_hash_sha256_BYTES));
  }

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
