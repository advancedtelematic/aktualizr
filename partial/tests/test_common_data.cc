#include "common_data_api.h"

#include <set>

#include "ed25519/sha512.h"

#define TOKEN_POOL_SIZE 100

extern "C" {
jsmntok_t token_pool[TOKEN_POOL_SIZE];
const unsigned int token_pool_size = TOKEN_POOL_SIZE;

#define SIGNATURE_POOL_SIZE 16
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

std::set<crypto_key_t*> allocated_keys;

crypto_key_t* alloc_crypto_key(void) {
  crypto_key_t *res = new crypto_key_t;
  allocated_keys.insert(res);
  return res;
}
void free_crypto_key(crypto_key_t* key) {
  allocated_keys.erase(key);
  delete key;
}
void free_all_crypto_keys() {
  for(auto key : allocated_keys) {
    delete key;
  }

  allocated_keys.clear();
}

struct crypto_hash_ctx {
  size_t bytes_fed;
  uint8_t block[SHA512_BLOCK_SIZE];
  struct sha512_state sha_state;
};

crypto_hash_ctx_t hash_context;

}
