#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <string>

struct PublicKey {
  PublicKey() {}
  PublicKey(const std::string &v, const std::string &t) : value(v), type(t) {}
  std::string value;
  std::string type;
};

class Crypto {
 public:
  static std::string sha256digest(const std::string &text);
  static std::string sha512digest(const std::string &text);
  static std::string RSAPSSSign(const std::string &private_key, const std::string &digest);
  static bool VerifySignature(const PublicKey &public_key, const std::string &signature, const std::string &message);
  static bool parseP12(FILE *p12_fp, const std::string &p12_password, const std::string &pkey_pem,
                       const std::string &client_pem, const std::string ca_pem);

 private:
  static bool RSAPSSVerify(const std::string &public_key, const std::string &signature, const std::string &message);
  static bool ED25519Verify(const std::string &public_key, const std::string &signature, const std::string &message);
};

#endif  // CRYPTO_H_