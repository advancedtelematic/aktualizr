#ifndef LIBUPTINY_COMMON_DATA_API_H
#define LIBUPTINY_COMMON_DATA_API_H

#include "crypto_api.h"
#include "jsmn/jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

extern jsmntok_t token_pool[];
extern const unsigned int token_pool_size;

extern crypto_key_and_signature_t signature_pool[];
extern const unsigned int signature_pool_size;

extern crypto_verify_ctx_t crypto_ctx_pool[];
extern const unsigned int crypto_ctx_pool_size;

crypto_key_t* alloc_crypto_key(void);
void free_crypto_key(crypto_key_t* key);
void free_all_crypto_keys();

#ifdef __cplusplus
}
#endif

#endif  // LIBUPTINY_COMMON_DATA_API_H
