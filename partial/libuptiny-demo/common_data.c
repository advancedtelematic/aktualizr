#include "libuptiny/common_data_api.h"

#include "ed25519/sha512.h"

#define TOKEN_POOL_SIZE 50

jsmntok_t token_pool[TOKEN_POOL_SIZE];
const int16_t token_pool_size = TOKEN_POOL_SIZE;

#define SIGNATURE_POOL_SIZE 4
crypto_key_and_signature_t signature_pool[SIGNATURE_POOL_SIZE];
const unsigned int signature_pool_size = SIGNATURE_POOL_SIZE;

/* TODO: Duplicated from test_crypto.cc, something needs to be done about it */
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

crypto_hash_ctx_t hash_context;

#define CRYPTO_CONTEXT_POOL_SIZE 4
crypto_verify_ctx_t crypto_ctx_pool_data[CRYPTO_CONTEXT_POOL_SIZE];
crypto_verify_ctx_t* crypto_ctx_pool[] = {
  &crypto_ctx_pool_data[0],
  &crypto_ctx_pool_data[1],
  &crypto_ctx_pool_data[2],
  &crypto_ctx_pool_data[3],
};

const unsigned int crypto_ctx_pool_size = CRYPTO_CONTEXT_POOL_SIZE;

#define KEY_POOL_SIZE 16
crypto_key_t key_pool[KEY_POOL_SIZE];
uint32_t key_pool_mark = 0;

crypto_key_t* alloc_crypto_key(void) {
  for (int i = 0; i < KEY_POOL_SIZE; ++i) {
    if(!(key_pool_mark & (1 << i))) {
      key_pool_mark |= (1 << i);
      return &key_pool[i];
    }
  }
  return NULL;
}
void free_crypto_key(crypto_key_t* key) {
  int idx = ((uint32_t) key - (uint32_t)&key_pool) / sizeof(crypto_key_t);
  if (idx < KEY_POOL_SIZE) {
    key_pool_mark &= ~(1 << idx);
  }
}
void free_all_crypto_keys(void) {
  key_pool_mark = 0;
}
