#include "crypto_common.h"
#include "debug.h"
#include "utils.h"

crypto_key_t* find_key(const char* key_id, int len, crypto_key_t** keys, int num_keys) {
  if (len != (CRYPTO_KEYID_LEN * 2)) {
    return NULL;
  }

  for (int i = 0; i < num_keys; i++) {
    if (hex_bin_cmp(key_id, len, keys[i]->keyid) == 0) {
      return keys[i];
    }
  }

  DEBUG_PRINTF("Key not found: %.*s\n", len, key_id);
  return NULL;
}
