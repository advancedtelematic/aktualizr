#ifndef P11_H_
#define P11_H_

#include <libp11.h>
#include <string>
#include "config.h"

class P11 {
 public:
  P11(const P11Config &config);
  bool readPublicKey(std::string *public_key);
  ~P11();

 private:
  void clean();
  P11Config config_;
  unsigned int nslots_;
  PKCS11_SLOT *slot_ = NULL;
  PKCS11_SLOT *slots_ = NULL;
  PKCS11_CTX *ctx_ = NULL;
};

#endif  // P11_H_
