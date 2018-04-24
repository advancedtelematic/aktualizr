#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <sodium.h>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <string>

#include "utilities/types.h"
#include "utilities/utils.h"

// some older versions of openssl have BIO_new_mem_buf defined with fisrt parameter of type (void*)
//   which is not true and breaks our build
#undef BIO_new_mem_buf
BIO *BIO_new_mem_buf(const void *, int);

struct PublicKey {
  enum Type { RSA = 0, ED25519 };
  PublicKey() {}
  PublicKey(const boost::filesystem::path &path) {
    value = Utils::readFile(path.string());
    value = value.substr(0, value.size() - 1);
    // value = boost::replace_all_copy(value, "\n", "\\n");
    type = RSA;  // temporary suuport only RSA
  }
  PublicKey(const std::string &v, const std::string &t) : value(v) {
    std::string type_str = boost::algorithm::to_lower_copy(t);
    if (type_str == "rsa") {
      type = RSA;
      BIO *bufio = BIO_new_mem_buf((const void *)value.c_str(), (int)value.length());
      ::RSA *rsa = PEM_read_bio_RSA_PUBKEY(bufio, 0, 0, 0);
      key_length = RSA_size(rsa);
      RSA_free(rsa);
      BIO_free_all(bufio);
    } else if (type_str == "ed25519") {
      type = ED25519;
    } else {
      throw std::runtime_error("unsupported key type: " + t);
    }
  }
  std::string TypeString() const;
  bool operator==(const PublicKey &rhs) const {
    return value == rhs.value && type == rhs.type && (type != RSA || key_length == rhs.key_length);
  }
  std::string value;
  Type type;
  int key_length;
};

class MultiPartSHA512Hasher {
 public:
  MultiPartSHA512Hasher() { crypto_hash_sha512_init(&state_); }
  void update(const unsigned char *part, int64_t size) { crypto_hash_sha512_update(&state_, part, size); }
  std::string getHexDigest() {
    unsigned char sha512_hash[crypto_hash_sha512_BYTES];
    crypto_hash_sha512_final(&state_, sha512_hash);
    return boost::algorithm::hex(std::string((char *)sha512_hash, crypto_hash_sha512_BYTES));
  }

 private:
  crypto_hash_sha512_state state_;
};

class MultiPartSHA256Hasher {
 public:
  MultiPartSHA256Hasher() { crypto_hash_sha256_init(&state_); }
  void update(const unsigned char *part, int64_t size) { crypto_hash_sha256_update(&state_, part, size); }
  std::string getHexDigest() {
    unsigned char sha256_hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_final(&state_, sha256_hash);
    return boost::algorithm::hex(std::string((char *)sha256_hash, crypto_hash_sha256_BYTES));
  }

 private:
  crypto_hash_sha256_state state_;
};

class Crypto {
 public:
  static std::string sha256digest(const std::string &text);
  static std::string sha512digest(const std::string &text);
  static std::string RSAPSSSign(ENGINE *engine, const std::string &private_key, const std::string &message);
  static std::string Sign(KeyType key_type, ENGINE *engine, const std::string &private_key, const std::string &message);
  static std::string ED25519Sign(const std::string &private_key, const std::string &message);

  static bool VerifySignature(const PublicKey &public_key, const std::string &signature, const std::string &message);
  static bool parseP12(BIO *p12_bio, const std::string &p12_password, std::string *out_pkey, std::string *out_cert,
                       std::string *out_ca);
  static bool extractSubjectCN(const std::string &cert, std::string *cn);
  static bool generateRSAKeyPair(KeyType key_type, std::string *public_key, std::string *private_key);
  static bool generateEDKeyPair(std::string *public_key, std::string *private_key);
  static bool generateKeyPair(KeyType key_type, std::string *public_key, std::string *private_key);

  static bool RSAPSSVerify(const std::string &public_key, const std::string &signature, const std::string &message);
  static bool ED25519Verify(const std::string &public_key, const std::string &signature, const std::string &message);
  static std::string getKeyId(const std::string &key);
};

#endif  // CRYPTO_H_
