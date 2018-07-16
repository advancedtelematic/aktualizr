#include "crypto_api.h"
#include <stdlib.h>
#include <string.h>
typedef int wint_t;
#include <strings.h>
#include "debug.h"
#include "ed25519/edsign.h"
#include "utils.h"

typedef struct {
  int sig_len;
  int pub_key_len;
  const char* str;
} alg_match_t;

typedef struct {
  int hash_len;
  const char* str;
} hash_match_t;

static alg_match_t keytypes[] = {{32, 64, "ed25519"}};

static hash_match_t hashtypes[] = {{64, "sha512"}};

crypto_algorithm_t crypto_str_to_keytype(const char* keytype, int len) {
  for (int i = 0; i < sizeof(keytypes) / sizeof(keytypes[0]); i++) {
    if (!strncasecmp(keytype, keytypes[i].str, len)) {
      return i;
    }
  }

  return CRYPTO_ALG_UNKNOWN;
}

crypto_hash_algorithm_t crypto_str_to_hashtype(const char* hashtype, int len) {
  for (int i = 0; i < sizeof(hashtypes) / sizeof(hashtypes[0]); i++) {
    if (!strncasecmp(hashtype, hashtypes[i].str, len)) {
      return i;
    }
  }

  return CRYPTO_HASH_UNKNOWN;
}


void crypto_verify_init(crypto_verify_ctx_t* ctx, crypto_key_and_signature_t* sig) {
  ctx->bytes_fed = 0;
  ctx->signature = sig->sig;
  ctx->pub = sig->key->keyval;
}

void crypto_verify_feed(crypto_verify_ctx_t* ctx, const uint8_t* data, int len) {
  int i;

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

int crypto_get_hashlen(crypto_hash_algorithm_t alg) { return hashtypes[alg].hash_len; }

int crypto_get_keylen(crypto_algorithm_t alg) { return keytypes[alg].pub_key_len; }
