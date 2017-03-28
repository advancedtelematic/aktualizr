#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <string>

class Crypto {
 public:
  static std::string sha256digest(const std::string &text);
  static std::string RSAPSSSign(const std::string &private_key, const std::string &digest);
  static bool RSAPSSVerify(const std::string &public_key, const std::string &signature, const std::string &message);
  static bool parseP12(FILE *p12_fp, const std::string &p12_password, const std::string &pkey_pem,
                       const std::string &client_pem, const std::string ca_pem);
};

#endif  // CRYPTO_H_