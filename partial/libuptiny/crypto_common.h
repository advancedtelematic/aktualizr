#ifndef LIBUPTINY_CRYPTO_COMMON_H_
#define LIBUPTINY_CRYPTO_COMMON_H_

#include "crypto_api.h"

crypto_key_t* find_key(const char* key_id, int len, crypto_key_t** keys, int num_keys);

#endif  // LIBUPTINY_CRYPTO_COMMON_H_
