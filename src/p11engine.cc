#include "p11engine.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <boost/scoped_array.hpp>

#include "utils.h"

#ifdef TEST_PKCS11_ENGINE_PATH
#define kPkcs11Path TEST_PKCS11_ENGINE_PATH
#else
#define kPkcs11Path "/usr/lib/engines/libpkcs11.so"
#endif

P11Engine* P11Engine::instance = NULL;
P11Engine* P11Engine::Get(const P11Config& config) {
  if (instance == NULL) {
    instance = new P11Engine(config);
  }
  return instance;
}

P11Engine::P11Engine(const P11Config& config) : config_(config), ctx_(config_.module), slots_(ctx_.get()) {
  if (config_.module.empty()) return;

  PKCS11_SLOT* slot = PKCS11_find_token(ctx_.get(), slots_.get_slots(), slots_.get_nslots());
  if (!slot || !slot->token) throw std::runtime_error("Couldn't find pkcs11 token");

  LOG_DEBUG << "Slot manufacturer......: " << slot->manufacturer;
  LOG_DEBUG << "Slot description.......: " << slot->description;
  LOG_DEBUG << "Slot token label.......: " << slot->token->label;
  LOG_DEBUG << "Slot token manufacturer: " << slot->token->manufacturer;
  LOG_DEBUG << "Slot token model.......: " << slot->token->model;
  LOG_DEBUG << "Slot token serialnr....: " << slot->token->serialnr;

  uri_prefix_ = std::string("pkcs11:serial=") + slot->token->serialnr + ";pin-value=" + config_.pass + ";id=%";

  ENGINE_load_builtin_engines();
  ssl_engine_ = ENGINE_by_id("dynamic");
  if (!ssl_engine_) throw std::runtime_error("SSL pkcs11 engine initialization failed");

  if (!ENGINE_ctrl_cmd_string(ssl_engine_, "SO_PATH", kPkcs11Path, 0))
    throw std::runtime_error(std::string("Engine command failed: SO_PATH ") + kPkcs11Path);

  if (!ENGINE_ctrl_cmd_string(ssl_engine_, "ID", "pkcs11", 0))
    throw std::runtime_error("Engine command failed: ID pksc11");

  if (!ENGINE_ctrl_cmd_string(ssl_engine_, "LIST_ADD", "1", 0))
    throw std::runtime_error("Engine command failed: LIST_ADD 1");

  if (!ENGINE_ctrl_cmd_string(ssl_engine_, "LOAD", NULL, 0)) throw std::runtime_error("Engine command failed: LOAD");

  if (!ENGINE_ctrl_cmd_string(ssl_engine_, "MODULE_PATH", config_.module.c_str(), 0))
    throw std::runtime_error(std::string("Engine command failed: MODULE_PATH ") + config_.module.string());

  if (!ENGINE_ctrl_cmd_string(ssl_engine_, "PIN", config_.pass.c_str(), 0))
    throw std::runtime_error(std::string("Engine command failed: PIN ") + config_.pass);

  if (!ENGINE_init(ssl_engine_)) throw std::runtime_error("Engine initialization failed");
}

P11Engine::~P11Engine() {
  if (!config_.module.empty()) {
    ENGINE_finish(ssl_engine_);
    ENGINE_cleanup();
  }
  P11Engine::instance = NULL;
}

PKCS11_SLOT* P11Engine::findTokenSlot() const {
  PKCS11_SLOT* slot = PKCS11_find_token(ctx_.get(), slots_.get_slots(), slots_.get_nslots());
  if (!slot || !slot->token) {
    LOG_ERROR << "Couldn't find a token";
    return NULL;
  }
  int rv;
  PKCS11_is_logged_in(slot, 1, &rv);
  if (rv == 0) {
    if (PKCS11_open_session(slot, 1)) {
      LOG_ERROR << "Error creating rw session in to the slot: " << ERR_error_string(ERR_get_error(), NULL);
    }

    if (PKCS11_login(slot, 0, config_.pass.c_str())) {
      LOG_ERROR << "Error logging in to the token: " << ERR_error_string(ERR_get_error(), NULL);
      return NULL;
    }
  }
  return slot;
}

bool P11Engine::readUptanePublicKey(std::string* key_out) {
  if (config_.module.empty()) return false;
  if (config_.uptane_key_id.length() % 2) return false;  // id is a hex string

  PKCS11_SLOT* slot = findTokenSlot();
  if (!slot) return false;

  PKCS11_KEY* keys;
  unsigned int nkeys;
  int rc = PKCS11_enumerate_public_keys(slot->token, &keys, &nkeys);
  if (rc < 0) {
    LOG_ERROR << "Error enumerating public keys in PKCS11 device: " << ERR_error_string(ERR_get_error(), NULL);
    return false;
  }
  PKCS11_KEY* key = NULL;
  {
    boost::scoped_array<unsigned char> id_hex(new unsigned char[config_.uptane_key_id.length() / 2]);
    Utils::hex2bin(config_.uptane_key_id, id_hex.get());

    for (int i = 0; i < nkeys; i++) {
      if ((keys[i].id_len == config_.uptane_key_id.length() / 2) &&
          !memcmp(keys[i].id, id_hex.get(), config_.uptane_key_id.length() / 2)) {
        key = &keys[i];
        break;
      }
    }
  }
  if (key == NULL) {
    LOG_ERROR << "Requested public key was not found";
    return false;
  }
  EVP_PKEY* evp_key = PKCS11_get_public_key(key);
  BIO* mem = BIO_new(BIO_s_mem());
  PEM_write_bio_PUBKEY(mem, evp_key);

  char* pem_key = NULL;
  int length = BIO_get_mem_data(mem, &pem_key);
  key_out->assign(pem_key, length);

  BIO_free_all(mem);
  return true;
}

bool P11Engine::generateUptaneKeyPair() {
  PKCS11_SLOT* slot = findTokenSlot();
  if (!slot) return false;

  boost::scoped_array<unsigned char> id_hex(new unsigned char[config_.uptane_key_id.length() / 2]);
  Utils::hex2bin(config_.uptane_key_id, id_hex.get());

  if (PKCS11_generate_key(slot->token, EVP_PKEY_RSA, 2048, NULL, id_hex.get(), (config_.uptane_key_id.length() / 2))) {
    LOG_ERROR << "Error of generating keypair on the device:" << ERR_error_string(ERR_get_error(), NULL);

    return false;
  }

  return true;
}

bool P11Engine::readTlsCert(std::string* cert_out) const {
  const std::string& id = config_.tls_clientcert_id;

  if (config_.module.empty()) return false;
  if (id.length() % 2) return false;  // id is a hex string

  PKCS11_SLOT* slot = findTokenSlot();
  if (!slot) return false;

  PKCS11_CERT* certs;
  unsigned int ncerts;
  int rc = PKCS11_enumerate_certs(slot->token, &certs, &ncerts);
  if (rc < 0) {
    LOG_ERROR << "Error enumerating certificates in PKCS11 device: " << ERR_error_string(ERR_get_error(), NULL);
    return false;
  }

  PKCS11_CERT* cert = NULL;
  {
    boost::scoped_array<unsigned char> id_hex(new unsigned char[id.length() / 2]);
    Utils::hex2bin(id, id_hex.get());

    for (int i = 0; i < ncerts; i++) {
      if ((certs[i].id_len == id.length() / 2) && !memcmp(certs[i].id, id_hex.get(), id.length() / 2)) {
        cert = &certs[i];
        break;
      }
    }
  }
  if (cert == NULL) {
    LOG_ERROR << "Requested certificate was not found";
    return false;
  }
  BIO* mem = BIO_new(BIO_s_mem());
  PEM_write_bio_X509(mem, cert->x509);

  char* pem_key = NULL;
  int length = BIO_get_mem_data(mem, &pem_key);
  cert_out->assign(pem_key, length);

  BIO_free_all(mem);
  return true;
}
