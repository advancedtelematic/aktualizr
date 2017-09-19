#include "p11.h"

#include <openssl/pem.h>
#include <stdexcept>

#include "logger.h"
#include "utils.h"

P11::P11(const P11Config &config) : config_(config) {
  ctx_ = PKCS11_CTX_new();
  int rc = PKCS11_CTX_load(ctx_, config_.module.c_str());
  if (rc) {
    PKCS11_CTX_free(ctx_);
    throw std::runtime_error(std::string("Cannot load ") + config_.module + " module");
  }
  rc = PKCS11_enumerate_slots(ctx_, &slots_, &nslots_);

  if (config_.slot + 1 > nslots_) {
    clean();
    throw std::runtime_error(std::string("No slot ") + Utils::intToString(config_.slot));
  }

  slot_ = PKCS11_find_token(ctx_, &slots_[config_.slot], nslots_);
  if (!slot_ || !slot_->token) {
    clean();
    throw std::runtime_error("No token available");
  }

  if (slot_->token->loginRequired) {
    if (PKCS11_login(slot_, 0, config_.pin.c_str()) != 0) {
      clean();
      throw std::runtime_error("Could not login to tokin with pin provided");
    }
  }
  LOGGER_LOG(LVL_info, "Using slot token with label: " << slot_->token->label);
}

bool P11::readPublicKey(std::string *public_key) {
  PKCS11_KEY *key;
  unsigned int nkeys;
  PKCS11_enumerate_public_keys(slot_->token, &key, &nkeys);
  if (nkeys < 1) {
    return false;
  }
  EVP_PKEY *evp_key = PKCS11_get_public_key(key);
  BIO *mem = BIO_new(BIO_s_mem());
  PEM_write_bio_PUBKEY(mem, evp_key);

  char *pem_key = NULL;
  int length = BIO_get_mem_data(mem, &pem_key);
  public_key->assign(pem_key, length);

  BIO_free_all(mem);
  return true;
}

void P11::clean() {
  PKCS11_release_all_slots(ctx_, slots_, nslots_);
  PKCS11_CTX_unload(ctx_);
  PKCS11_CTX_free(ctx_);
}

P11::~P11() { clean(); }
