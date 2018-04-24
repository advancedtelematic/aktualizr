#include "utilities/p11engine.h"

#include <libp11.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <boost/algorithm/hex.hpp>
#include <boost/scoped_array.hpp>
#include <utility>
#include <vector>

#include "utilities/config_utils.h"
#include "utilities/utils.h"

#ifdef TEST_PKCS11_ENGINE_PATH
#define kPkcs11Path TEST_PKCS11_ENGINE_PATH
#else
#define kPkcs11Path "/usr/lib/engines/pkcs11.so"
#endif

P11Engine* P11EngineGuard::instance = nullptr;
int P11EngineGuard::ref_counter = 0;

P11ContextWrapper::P11ContextWrapper(const boost::filesystem::path& module) {
  if (module.empty()) {
    ctx = nullptr;
    return;
  }
  // never returns NULL
  ctx = PKCS11_CTX_new();
  if (PKCS11_CTX_load(ctx, module.c_str()) != 0) {
    PKCS11_CTX_free(ctx);
    LOG_ERROR << "Couldn't load PKCS11 module " << module.string() << ": "
              << ERR_error_string(ERR_get_error(), nullptr);
    throw std::runtime_error("PKCS11 error");
  }
}

P11ContextWrapper::~P11ContextWrapper() {
  if (ctx != nullptr) {
    PKCS11_CTX_unload(ctx);
    PKCS11_CTX_free(ctx);
  }
}

P11SlotsWrapper::P11SlotsWrapper(PKCS11_ctx_st* ctx_in) {
  ctx = ctx_in;
  if (ctx == nullptr) {
    slots = nullptr;
    nslots = 0;
    return;
  }
  if (PKCS11_enumerate_slots(ctx, &slots, &nslots) != 0) {
    LOG_ERROR << "Couldn't enumerate slots"
              << ": " << ERR_error_string(ERR_get_error(), nullptr);
    throw std::runtime_error("PKCS11 error");
  }
}

P11SlotsWrapper::~P11SlotsWrapper() {
  if ((slots != nullptr) && (nslots != 0u)) {
    PKCS11_release_all_slots(ctx, slots, nslots);
  }
}

P11Engine::P11Engine(P11Config config) : config_(std::move(config)), ctx_(config_.module), slots_(ctx_.get()) {
  if (config_.module.empty()) {
    return;
  }

  PKCS11_SLOT* slot = PKCS11_find_token(ctx_.get(), slots_.get_slots(), slots_.get_nslots());
  if ((slot == nullptr) || (slot->token == nullptr)) {
    throw std::runtime_error("Couldn't find pkcs11 token");
  }

  LOG_DEBUG << "Slot manufacturer......: " << slot->manufacturer;
  LOG_DEBUG << "Slot description.......: " << slot->description;
  LOG_DEBUG << "Slot token label.......: " << slot->token->label;
  LOG_DEBUG << "Slot token manufacturer: " << slot->token->manufacturer;
  LOG_DEBUG << "Slot token model.......: " << slot->token->model;
  LOG_DEBUG << "Slot token serialnr....: " << slot->token->serialnr;

  uri_prefix_ = std::string("pkcs11:serial=") + slot->token->serialnr + ";pin-value=" + config_.pass + ";id=%";

  ENGINE_load_builtin_engines();
  ssl_engine_ = ENGINE_by_id("dynamic");
  if (ssl_engine_ == nullptr) {
    throw std::runtime_error("SSL pkcs11 engine initialization failed");
  }

  if (ENGINE_ctrl_cmd_string(ssl_engine_, "SO_PATH", kPkcs11Path, 0) == 0) {
    throw std::runtime_error(std::string("Engine command failed: SO_PATH ") + kPkcs11Path);
  }

  if (ENGINE_ctrl_cmd_string(ssl_engine_, "ID", "pkcs11", 0) == 0) {
    throw std::runtime_error("Engine command failed: ID pksc11");
  }

  if (ENGINE_ctrl_cmd_string(ssl_engine_, "LIST_ADD", "1", 0) == 0) {
    throw std::runtime_error("Engine command failed: LIST_ADD 1");
  }

  if (ENGINE_ctrl_cmd_string(ssl_engine_, "LOAD", nullptr, 0) == 0) {
    throw std::runtime_error("Engine command failed: LOAD");
  }

  if (ENGINE_ctrl_cmd_string(ssl_engine_, "MODULE_PATH", config_.module.c_str(), 0) == 0) {
    throw std::runtime_error(std::string("Engine command failed: MODULE_PATH ") + config_.module.string());
  }

  if (ENGINE_ctrl_cmd_string(ssl_engine_, "PIN", config_.pass.c_str(), 0) == 0) {
    throw std::runtime_error(std::string("Engine command failed: PIN ") + config_.pass);
  }

  if (ENGINE_init(ssl_engine_) == 0) {
    throw std::runtime_error("Engine initialization failed");
  }
}

P11Engine::~P11Engine() {
  if (!config_.module.empty()) {
    ENGINE_finish(ssl_engine_);
    ENGINE_cleanup();
  }
}

PKCS11_SLOT* P11Engine::findTokenSlot() const {
  PKCS11_SLOT* slot = PKCS11_find_token(ctx_.get(), slots_.get_slots(), slots_.get_nslots());
  if ((slot == nullptr) || (slot->token == nullptr)) {
    LOG_ERROR << "Couldn't find a token";
    return nullptr;
  }
  int rv;
  PKCS11_is_logged_in(slot, 1, &rv);
  if (rv == 0) {
    if (PKCS11_open_session(slot, 1) != 0) {
      LOG_ERROR << "Error creating rw session in to the slot: " << ERR_error_string(ERR_get_error(), nullptr);
    }

    if (PKCS11_login(slot, 0, config_.pass.c_str()) != 0) {
      LOG_ERROR << "Error logging in to the token: " << ERR_error_string(ERR_get_error(), nullptr);
      return nullptr;
    }
  }
  return slot;
}

bool P11Engine::readUptanePublicKey(std::string* key_out) {
  if (config_.module.empty()) {
    return false;
  }
  if ((config_.uptane_key_id.length() % 2) != 0u) {
    return false;  // id is a hex string
  }

  PKCS11_SLOT* slot = findTokenSlot();
  if (slot == nullptr) {
    return false;
  }

  PKCS11_KEY* keys;
  unsigned int nkeys;
  int rc = PKCS11_enumerate_public_keys(slot->token, &keys, &nkeys);
  if (rc < 0) {
    LOG_ERROR << "Error enumerating public keys in PKCS11 device: " << ERR_error_string(ERR_get_error(), nullptr);
    return false;
  }
  PKCS11_KEY* key = nullptr;
  {
    std::vector<unsigned char> id_hex;
    boost::algorithm::unhex(config_.uptane_key_id, std::back_inserter(id_hex));

    for (int i = 0; i < nkeys; i++) {
      if ((keys[i].id_len == config_.uptane_key_id.length() / 2) &&
          (memcmp(keys[i].id, id_hex.data(), config_.uptane_key_id.length() / 2) == 0)) {
        key = &keys[i];
        break;
      }
    }
  }
  if (key == nullptr) {
    LOG_ERROR << "Requested public key was not found";
    return false;
  }
  StructGuard<EVP_PKEY> evp_key(PKCS11_get_public_key(key), EVP_PKEY_free);
  StructGuard<BIO> mem(BIO_new(BIO_s_mem()), BIO_vfree);
  PEM_write_bio_PUBKEY(mem.get(), evp_key.get());

  char* pem_key = nullptr;
  int length = BIO_get_mem_data(mem.get(), &pem_key);  // NOLINT
  key_out->assign(pem_key, length);

  return true;
}

bool P11Engine::generateUptaneKeyPair() {
  PKCS11_SLOT* slot = findTokenSlot();
  if (slot == nullptr) {
    return false;
  }

  std::vector<unsigned char> id_hex;
  boost::algorithm::unhex(config_.uptane_key_id, std::back_inserter(id_hex));

// PKCS11_generate_key() is deprecated in some versions of libp11, but has
// been re-instated as of commit b1edde63c1738cd321793d04a7ed7cbd357c9f90 on
// Oct 19th 2017
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if (PKCS11_generate_key(slot->token, EVP_PKEY_RSA, 2048, nullptr, id_hex.data(),
                          (config_.uptane_key_id.length() / 2)) != 0) {
    LOG_ERROR << "Error of generating keypair on the device:" << ERR_error_string(ERR_get_error(), nullptr);

    return false;
  }
#pragma GCC diagnostic pop
  return true;
}

bool P11Engine::readTlsCert(std::string* cert_out) const {
  const std::string& id = config_.tls_clientcert_id;

  if (config_.module.empty()) {
    return false;
  }
  if ((id.length() % 2) != 0u) {
    return false;  // id is a hex string
  }

  PKCS11_SLOT* slot = findTokenSlot();
  if (slot == nullptr) {
    return false;
  }

  PKCS11_CERT* certs;
  unsigned int ncerts;
  int rc = PKCS11_enumerate_certs(slot->token, &certs, &ncerts);
  if (rc < 0) {
    LOG_ERROR << "Error enumerating certificates in PKCS11 device: " << ERR_error_string(ERR_get_error(), nullptr);
    return false;
  }

  PKCS11_CERT* cert = nullptr;
  {
    std::vector<unsigned char> id_hex;
    boost::algorithm::unhex(id, std::back_inserter(id_hex));

    for (int i = 0; i < ncerts; i++) {
      if ((certs[i].id_len == id.length() / 2) && (memcmp(certs[i].id, id_hex.data(), id.length() / 2) == 0)) {
        cert = &certs[i];
        break;
      }
    }
  }
  if (cert == nullptr) {
    LOG_ERROR << "Requested certificate was not found";
    return false;
  }
  StructGuard<BIO> mem(BIO_new(BIO_s_mem()), BIO_vfree);
  PEM_write_bio_X509(mem.get(), cert->x509);

  char* pem_key = nullptr;
  int length = BIO_get_mem_data(mem.get(), &pem_key);  // NOLINT
  cert_out->assign(pem_key, length);

  return true;
}
