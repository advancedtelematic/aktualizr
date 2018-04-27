#ifndef P11ENGINE_H_
#define P11ENGINE_H_

#include <openssl/engine.h>
#include <openssl/err.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "logging/logging.h"
#include "utilities/config_utils.h"

// declare p11 types as incomplete so that the header can be used without libp11
struct PKCS11_ctx_st;
struct PKCS11_slot_st;

struct P11Config {
  boost::filesystem::path module;
  std::string pass;
  std::string uptane_key_id;
  std::string tls_cacert_id;
  std::string tls_pkey_id;
  std::string tls_clientcert_id;

  void updateFromPropertyTree(const boost::property_tree::ptree &pt) {
    CopyFromConfig(module, "module", boost::log::trivial::trace, pt);
    CopyFromConfig(pass, "pass", boost::log::trivial::trace, pt);
    CopyFromConfig(uptane_key_id, "uptane_key_id", boost::log::trivial::trace, pt);
    CopyFromConfig(tls_cacert_id, "tls_cacert_id", boost::log::trivial::trace, pt);
    CopyFromConfig(tls_pkey_id, "tls_pkey_id", boost::log::trivial::trace, pt);
    CopyFromConfig(tls_clientcert_id, "tls_clientcert_id", boost::log::trivial::trace, pt);
  }

  void writeToStream(std::ostream &out_stream) const {
    writeOption(out_stream, module, "module");
    writeOption(out_stream, pass, "pass");
    writeOption(out_stream, uptane_key_id, "uptane_key_id");
    writeOption(out_stream, tls_cacert_id, "tls_ca_id");
    writeOption(out_stream, tls_pkey_id, "tls_pkey_id");
    writeOption(out_stream, tls_clientcert_id, "tls_clientcert_id");
  }
};

class P11ContextWrapper {
 public:
  explicit P11ContextWrapper(const boost::filesystem::path &module);
  ~P11ContextWrapper();
  P11ContextWrapper(const P11ContextWrapper &) = delete;
  P11ContextWrapper &operator=(const P11ContextWrapper &) = delete;
  PKCS11_ctx_st *get() const { return ctx; }

 private:
  PKCS11_ctx_st *ctx;
};

class P11SlotsWrapper {
 public:
  explicit P11SlotsWrapper(PKCS11_ctx_st *ctx_in);
  ~P11SlotsWrapper();
  P11SlotsWrapper(const P11SlotsWrapper &) = delete;
  P11SlotsWrapper &operator=(const P11SlotsWrapper &) = delete;
  PKCS11_slot_st *get_slots() const { return slots; }
  unsigned int get_nslots() const { return nslots; }

 private:
  PKCS11_ctx_st *ctx;
  PKCS11_slot_st *slots;
  unsigned int nslots;
};

class P11EngineGuard;

class P11Engine {
 public:
  P11Engine(const P11Engine &) = delete;
  P11Engine &operator=(const P11Engine &) = delete;

  ~P11Engine();
  ENGINE *getEngine() { return ssl_engine_; }
  std::string getUptaneKeyId() const { return uri_prefix_ + config_.uptane_key_id; }
  std::string getTlsCacertId() const { return uri_prefix_ + config_.tls_cacert_id; }
  std::string getTlsPkeyId() const { return uri_prefix_ + config_.tls_pkey_id; }
  std::string getTlsCertId() const { return uri_prefix_ + config_.tls_clientcert_id; }
  bool readUptanePublicKey(std::string *key_out);
  bool readTlsCert(std::string *cert_out) const;
  bool generateUptaneKeyPair();

 private:
  const P11Config config_;
  std::string uri_prefix_;
  ENGINE *ssl_engine_;
  P11ContextWrapper ctx_;
  P11SlotsWrapper slots_;

  PKCS11_slot_st *findTokenSlot() const;

  explicit P11Engine(P11Config config);

  friend class P11EngineGuard;
};

class P11EngineGuard {
 public:
  explicit P11EngineGuard(const P11Config &config) {
    if (instance == nullptr) {
      instance = new P11Engine(config);
    }
    ++ref_counter;
  };
  ~P11EngineGuard() {
    if (ref_counter != 0) {
      --ref_counter;
    }
    if (ref_counter == 0) {
      delete instance;
      instance = nullptr;
    }
  }
  P11Engine *operator->() const { return instance; }

 private:
  static P11Engine *instance;
  static int ref_counter;
};

#endif
