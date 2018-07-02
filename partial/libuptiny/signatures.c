/*
  Signatures JSON value:
  [
    {
      "keyid" : "<keyid>",
      "method" : "<method>",
      "sig" : <base64-encoded signature>
    },
    {
      "keyid": ...
    },
    ...
  ]
*/

#include "signatures.h"
#include "base64.h"
#include "crypto.h"
#include "debug.h"
#include "json_common.h"
#include "utils.h"
#include "jsmn/jsmn.h"

static jsmn_parser parser;
extern jsmntok_t token_pool[];
extern const unsigned int token_pool_size;

crypto_key_t **meta_keys;
int meta_keys_num;

static crypto_key_t *find_key(const char* key_id, int len) {
  if (len != (CRYPTO_KEYID_LEN * 2)) {
    return NULL;
  }

  for(int i = 0; i < meta_keys_num; i++) {
    if(hex_bin_cmp(key_id, len, meta_keys[i]->keyid) == 0) {
      return meta_keys[i];
    }
  }
  
  DEBUG_PRINTF("Key not found: %.*s\n", len, key_id);
  return NULL;
}

static inline int parse_sig (const char *json_sig, unsigned int *pos, crypto_key_and_signature_t* sig) {
  unsigned int idx = *pos;

  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return -1;
  }

  bool key_found = false;
  bool sig_found = false;
  int size = token_pool[idx].size;
  uint8_t sig_buf[CRYPTO_SIGNATURE_LEN + 2]; // '+2' because base64 operates in 3 byte granularity
  ++idx; // consume object token
  for (int i = 0; i < size; ++i) {
    if ((token_pool[idx].end - token_pool[idx].start) == 5 && (strncmp("keyid", json_sig+token_pool[idx].start, 5) == 0)) {
      ++idx; //  consume name token
      if (token_pool[idx].type != JSMN_STRING) {
          DEBUG_PRINTF("Key ID is not a string\n");
          idx = consume_recursive_json(idx);
      } else {
        crypto_key_t* key = find_key(json_sig + token_pool[idx].start, token_pool[idx].end - token_pool[idx].start);
        if (key) {
          sig->key = key;
          key_found = true;
        }
        ++idx; // consume key
      }
    } else if((token_pool[idx].end - token_pool[idx].start) == 6 && (strncmp("method", json_sig+token_pool[idx].start, 6) == 0)) {
      // method is ignored for now
      ++idx; //  consume name token
      idx = consume_recursive_json(idx); // consume value
    } else if((token_pool[idx].end - token_pool[idx].start) == 3 && (strncmp("sig", json_sig+token_pool[idx].start, 3) == 0)) {
      ++idx; //  consume name token
      if (token_pool[idx].type != JSMN_STRING) {
          DEBUG_PRINTF("Signature is not a string\n");
          idx = consume_recursive_json(idx);
      } else {
        int b64_len = token_pool[idx].end - token_pool[idx].start;
        if (BASE64_DECODED_BUF_SIZE(b64_len) <= CRYPTO_SIGNATURE_LEN+2) {
          if(CRYPTO_SIGNATURE_LEN == base64_decode(json_sig + token_pool[idx].start, b64_len, sig_buf)) {
            memcpy(sig->sig, sig_buf, CRYPTO_SIGNATURE_LEN);
            sig_found = true;
          } else {
            DEBUG_PRINTF("Unexpected signature size\n");
          }
        } else {
          DEBUG_PRINTF("Signature is too large\n");
        }
        ++idx; // consume signature
      }
    } else {
      DEBUG_PRINTF("Unknown field in a signature: \"%.*s\"\n", token_pool[i].end-token_pool[i].start, json_sig+token_pool[i].start); 
      ++idx; //  consume name token
      idx = consume_recursive_json(idx); // consume value
    }
  }

  *pos = idx;
  return (sig_found && key_found);
}

int uptane_parse_signatures(uptane_role_t role, const char *signatures, size_t len, crypto_key_and_signature_t *output, int max_sigs) {
  const uptane_root_t* root_meta = state_get_root();
  if(role == ROLE_ROOT) {
    meta_keys = root_meta->root_keys;
    meta_keys_num = root_meta->root_keys_num;
  } else { // ROLE_TARGETS
    meta_keys = root_meta->targets_keys;
    meta_keys_num = root_meta->targets_keys_num;
  }

  jsmn_init(&parser);

  int parsed = jsmn_parse(&parser, signatures, len, token_pool, token_pool_size);
  
  if (parsed < 0) {
    DEBUG_PRINTF("Failed to parse signatures\n");
    return -1;
  }

  if (parsed < 1 || token_pool[0].type != JSMN_ARRAY) {
    DEBUG_PRINTF("Array expected\n");
    return -1;
  }

  int sigs_read = 0;
  unsigned int token_idx = 1;
  int array_size = token_pool[0].size;

  for (int i = 0; i < array_size; ++i) {
    if (sigs_read >= max_sigs) {
      return sigs_read;
    }

    int res = parse_sig(signatures, &token_idx, output+sigs_read);
    if (res < 0) {
      return -1;
    } else if (res != 0){
      ++sigs_read;
    }
  }
  return sigs_read;
}
