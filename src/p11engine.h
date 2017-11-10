#ifndef P11ENGINE_H_
#define P11ENGINE_H_

#include <libp11.h>
#include <openssl/engine.h>
#include <openssl/err.h>

#include "config.h"
#include "logger.h"

class P11ContextWrapper {
 public:
  P11ContextWrapper(const std::string &module) {
    if (module.empty()) {
      ctx = NULL;
      return;
    }
    // never returns NULL
    ctx = PKCS11_CTX_new();
    if (PKCS11_CTX_load(ctx, module.c_str())) {
      PKCS11_CTX_free(ctx);
      LOGGER_LOG(LVL_error, "Couldn't load PKCS11 module " << module << ": "
                                                           << ERR_error_string(ERR_get_error(), NULL));
      throw std::runtime_error("PKCS11 error");
    }
  }
  ~P11ContextWrapper() {
    if (ctx) {
      PKCS11_CTX_unload(ctx);
      PKCS11_CTX_free(ctx);
    }
  }
  PKCS11_CTX *get() { return ctx; }

 private:
  PKCS11_CTX *ctx;
};

class P11SlotsWrapper {
 public:
  P11SlotsWrapper(PKCS11_CTX *ctx_in) {
    ctx = ctx_in;
    if (!ctx) {
      slots = NULL;
      nslots = 0;
      return;
    }
    if (PKCS11_enumerate_slots(ctx, &slots, &nslots)) {
      LOGGER_LOG(LVL_error, "Couldn't enumerate slots"
                                << ": " << ERR_error_string(ERR_get_error(), NULL));
      throw std::runtime_error("PKCS11 error");
    }
  }
  ~P11SlotsWrapper() {
    if (slots && nslots) PKCS11_release_all_slots(ctx, slots, nslots);
  }
  PKCS11_SLOT *get_slots() { return slots; }
  unsigned int get_nslots() { return nslots; }

 private:
  PKCS11_CTX *ctx;
  PKCS11_SLOT *slots;
  unsigned int nslots;
};

class P11Engine {
 public:
  P11Engine(const P11Config &config);
  ~P11Engine();
  ENGINE *getEngine() { return ssl_engine_; }
  std::string getUriPrefix() const { return uri_prefix_; }
  bool readPublicKey(const std::string &id, std::string *key);
  bool readCert(const std::string &id, std::string *cert_out);
  bool generateRSAKeyPair(const std::string &id);

 private:
  const P11Config &config_;
  std::string uri_prefix_;
  ENGINE *ssl_engine_;
  P11ContextWrapper ctx_;
  P11SlotsWrapper slots_;

  PKCS11_SLOT *findTokenSlot();
};

#endif
