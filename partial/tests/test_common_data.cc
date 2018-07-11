#include "common_data_api.h"

#include <set>

#define TOKEN_POOL_SIZE 100

extern "C" {
jsmntok_t token_pool[TOKEN_POOL_SIZE];
const unsigned int token_pool_size = TOKEN_POOL_SIZE;

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

}
