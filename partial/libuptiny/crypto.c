#include "crypto.h"
#include "debug.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  crypto_algorithm_t alg;
  const char * str;
} alg_match_t;

static alg_match_t keytypes[] = { {ED25519, "ed25519"} };

crypto_algorithm_t crypto_str_to_keytype(const char* keytype, int len) {
  for(int i = 0; i < sizeof(keytypes)/sizeof(keytypes[0]); i++) {
    if(!strncasecmp(keytype, keytypes[i].str, len)) {
      return keytypes[i].alg;
    }
  }

  return CRYPTO_ALG_UNKNOWN;
}

crypto_key_t *find_key(const char* key_id, int len, crypto_key_t **keys, int num_keys) {
  if (len != (CRYPTO_KEYID_LEN * 2)) {
    return NULL;
  }

  for(int i = 0; i < num_keys; i++) {
    if(hex_bin_cmp(key_id, len, keys[i]->keyid) == 0) {
      return keys[i];
    }
  }
  
  DEBUG_PRINTF("Key not found: %.*s\n", len, key_id);
  return NULL;
}


