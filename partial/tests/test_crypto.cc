#include "crypto_api.h"
#include <stdlib.h>
#include <string.h>
//typedef int wint_t;
#include <strings.h>
#include "debug.h"
#include "ed25519/edsign.h"
#include "ed25519/sha512.h"
#include "utils.h"

#include "logging/logging.h"

extern "C" {
typedef struct {
  size_t sig_len;
  size_t pub_key_len;
  const char* str;
} alg_match_t;

typedef struct {
  size_t hash_len;
  const char* str;
} hash_match_t;

struct crypto_verify_ctx {
  size_t bytes_fed;
  uint8_t block[SHA512_BLOCK_SIZE];
  struct sha512_state sha_state;
  const uint8_t* signature;
  const uint8_t* pub;
};

struct crypto_hash_ctx {
  size_t bytes_fed;
  uint8_t block[SHA512_BLOCK_SIZE];
  struct sha512_state sha_state;
};

static alg_match_t keytypes[] = {{64, 32, "ed25519"}};

static hash_match_t hashtypes[] = {{64, "sha512"}};

crypto_algorithm_t crypto_str_to_keytype(const char* keytype, size_t len) {
  for (unsigned int i = 0; i < sizeof(keytypes) / sizeof(keytypes[0]); i++) {
    if (!strncasecmp(keytype, keytypes[i].str, len)) {
      return (crypto_algorithm_t) i;
    }
  }

  return CRYPTO_ALG_UNKNOWN;
}

crypto_hash_algorithm_t crypto_str_to_hashtype(const char* hashtype, size_t len) {
  for (unsigned int i = 0; i < sizeof(hashtypes) / sizeof(hashtypes[0]); i++) {
    if (!strncasecmp(hashtype, hashtypes[i].str, len)) {
      return (crypto_hash_algorithm_t) i;
    }
  }

  return CRYPTO_HASH_UNKNOWN;
}

void crypto_hash_init(crypto_hash_ctx_t* ctx) {
  ctx->bytes_fed = 0;
  sha512_init(&ctx->sha_state);
}

void crypto_hash_feed(crypto_hash_ctx_t* ctx, const uint8_t* data, size_t len) {
  unsigned int i;

  for (i = 0; i < len; i++) {
    /* Trust compiler to use masking instead of actual division */
    int ind = ctx->bytes_fed % SHA512_BLOCK_SIZE;
    ctx->block[ind] = data[i];

    if (ind == SHA512_BLOCK_SIZE - 1) {
      sha512_block(&ctx->sha_state, ctx->block);
    }
    ++ctx->bytes_fed;
  }
}

void crypto_hash_result(crypto_hash_ctx_t* ctx, crypto_hash_t* hash) {
  sha512_final(&ctx->sha_state, ctx->block, ctx->bytes_fed);
  sha512_get(&ctx->sha_state, hash->hash, 0, SHA512_HASH_SIZE);
}

void crypto_verify_init(crypto_verify_ctx_t* ctx, crypto_key_and_signature_t* sig) {
  ctx->bytes_fed = 0;
  ctx->signature = sig->sig;
  ctx->pub = sig->key->keyval;
}

void crypto_verify_feed(crypto_verify_ctx_t* ctx, const uint8_t* data, size_t len) {
  unsigned int i;

  for (i = 0; i < len; i++) {
    if (ctx->bytes_fed < SHA512_BLOCK_SIZE - 64) {
      ctx->block[ctx->bytes_fed++] = data[i];

      /* First block is ready */
      if (ctx->bytes_fed == SHA512_BLOCK_SIZE - 64) {
        edsign_verify_init(&ctx->sha_state, ctx->signature, ctx->pub, ctx->block, SHA512_BLOCK_SIZE - 64);
      }
    } else {
      /* Trust compiler to use masking instead of actual division */
      int ind = (ctx->bytes_fed - (SHA512_BLOCK_SIZE - 64)) % SHA512_BLOCK_SIZE;
      ctx->block[ind] = data[i];

      if (ind == SHA512_BLOCK_SIZE - 1) {
        edsign_verify_block(&ctx->sha_state, ctx->block);
      }
      ++ctx->bytes_fed;
    }
  }
}

bool crypto_verify_result(crypto_verify_ctx_t* ctx) {
  if (ctx->bytes_fed < SHA512_BLOCK_SIZE) {
    return edsign_verify_init(&ctx->sha_state, ctx->signature, ctx->pub, ctx->block, ctx->bytes_fed);
  } else {
    return edsign_verify_final(&ctx->sha_state, ctx->signature, ctx->pub, ctx->block, ctx->bytes_fed);
  }
}

size_t crypto_get_hashlen(crypto_hash_algorithm_t alg) { return hashtypes[alg].hash_len; }
size_t crypto_get_keylen(crypto_algorithm_t alg) { return keytypes[alg].pub_key_len; }
size_t crypto_get_siglen(crypto_algorithm_t alg) { return keytypes[alg].sig_len; }

void crypto_sign_data(const char* data, size_t len, crypto_key_and_signature_t* out_sig, const uint8_t* private_key) {
  LOG_ERROR << "SIGN_DATA DATA: " << std::string(data, len);
  LOG_ERROR << "SIGN_DATA KEYVAL: " << std::string((const char*)out_sig->key->keyval, 32);
  LOG_ERROR << "SIGN_DATA PRIVATE: " << std::string((const char* )private_key, 32);
  edsign_sign(out_sig->sig, out_sig->key->keyval, private_key, (const uint8_t*) data, len);
}

}
