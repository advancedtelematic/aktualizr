#include "root_signed.h"
#include "common_data_api.h"
#include "crypto_common.h"
#include "debug.h"
#include "jsmn/jsmn.h"
#include "json_common.h"
#include "utils.h"

static crypto_key_t *keys[ROOT_MAX_KEYS];
static int num_keys = 0;

static inline bool parse_keyval(const char *keyval, int len, crypto_key_t *key) {
  (void)keyval;
  (void)len;
  (void)key;

  switch (key->key_type) {
    case CRYPTO_ALG_ED25519:
      return hex2bin(keyval, len, key->keyval);
    default:
      return false;
  }
}

static inline bool parse_keys(const char *metadata_str, unsigned int *pos) {
  unsigned int idx = *pos;

  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return false;
  }
  int size = token_pool[idx].size;
  ++idx;  // consume object token

  for (int i = 0; i < size; ++i) {
    bool keytype_supported = false;
    bool keyval_found = false;

    keys[num_keys] = alloc_crypto_key();
    if (keys[num_keys] == NULL) {
      DEBUG_PRINTF("Couldn't allocate key\n");
      return false;
    }

    if (JSON_TOK_LEN(token_pool[idx]) != (2 * CRYPTO_KEYID_LEN)) {
      DEBUG_PRINTF("Invalid key ID length\n");
      idx = consume_recursive_json(idx + 1);  // consume key
      free_crypto_key(keys[num_keys]);
      continue;
    }

    if (!hex2bin(metadata_str + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), keys[num_keys]->keyid)) {
      DEBUG_PRINTF("Invalid key ID\n");
      idx = consume_recursive_json(idx + 1);  // consume key
      free_crypto_key(keys[num_keys]);
      continue;
    }

    idx++;  // consume key ID

    if (token_pool[idx].type != JSMN_OBJECT) {
      DEBUG_PRINTF("Invalid key object\n");
      idx = consume_recursive_json(idx);  // consume key
      free_crypto_key(keys[num_keys]);
    }

    int key_size = token_pool[idx].size;
    ++idx;  // consume object token

    for (int j = 0; j < key_size; ++j) {
      if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "keytype")) {
        ++idx;  // consume name token
        keys[num_keys]->key_type =
            crypto_str_to_keytype(metadata_str + token_pool[idx].start, (size_t)JSON_TOK_LEN(token_pool[idx]));
        keytype_supported = (keys[num_keys]->key_type != CRYPTO_ALG_UNKNOWN);
        ++idx;  // consume keytype
      } else if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "keyval")) {
        ++idx;  // consume name token
        if (token_pool[idx].type != JSMN_OBJECT) {
          DEBUG_PRINTF("Object expected\n");
          continue;
        }
        ++idx;  // consume object token

        int keyval_size = token_pool[idx].size;

        for (int k = 0; k < keyval_size; ++k) {
          if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "public")) {
            ++idx;  // consume name token

            keyval_found =
                parse_keyval(metadata_str + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), keys[num_keys]);
            ++idx;  // consume keyval
          } else {
            DEBUG_PRINTF("Unknown field in keyval object: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                         metadata_str + token_pool[idx].start);
            ++idx;                              //  consume name token
            idx = consume_recursive_json(idx);  // consume value
          }
        }
      } else {
        ++idx;  //  consume name token
        DEBUG_PRINTF("Unknown field in key object: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                     metadata_str + token_pool[idx].start);
        idx = consume_recursive_json(idx);  // consume value
      }
    }

    if (keytype_supported && keyval_found) {
      ++num_keys;
      if (num_keys >= ROOT_MAX_KEYS) {
        DEBUG_PRINTF("Too many keys in root.json");
        return false;
      }
    } else {
      free_crypto_key(keys[num_keys]);
    }
  }
  *pos = idx;
  return true;
}

static inline bool parse_role(const char *metadata_str, unsigned int *pos, int32_t *threshold, int32_t *out_key_num,
                              crypto_key_t **out_keys) {
  unsigned int idx = *pos;
  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return false;
  }
  int size = token_pool[idx].size;
  ++idx;  // consume object token

  bool threshold_found = false;
  bool keyids_found = false;

  for (int i = 0; i < size; ++i) {
    if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "threshold")) {
      ++idx;  // consume name token

      int32_t thr;
      if (dec2int(metadata_str + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), &thr) && thr >= 0) {
        *threshold = thr;
        threshold_found = true;
      }
      ++idx;  // consume threshold token
    } else if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "keyids")) {
      ++idx;  // consume name token
      if (token_pool[idx].type != JSMN_ARRAY) {
        DEBUG_PRINTF("keyids is not an array\n");
        idx = consume_recursive_json(idx);  // consume keyids
        continue;
      }
      int keyids_size = token_pool[idx].size;
      ++idx;  // consume array token
      keyids_found = true;
      int keyids_num = 0;

      for (int j = 0; j < keyids_size; j++) {
        crypto_key_t *key =
            find_key(metadata_str + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), keys, num_keys);
        if (key) {
          out_keys[keyids_num++] = key;
        }
        ++idx;  // consume keyid
      }
      *out_key_num = keyids_num;
    } else {
      ++idx;                              //  consume name token
      idx = consume_recursive_json(idx);  // consume value
    }
  }
  *pos = idx;
  return threshold_found && keyids_found;
}

static inline bool parse_roles(const char *metadata_str, unsigned int *pos, uptane_root_t *root) {
  unsigned int idx = *pos;
  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return false;
  }
  int size = token_pool[idx].size;
  ++idx;  // consume object token

  bool root_found = false;
  bool targets_found = false;

  for (int i = 0; i < size; ++i) {
    if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "root")) {
      ++idx;  // consume name token
      root_found = parse_role(metadata_str, &idx, &root->root_threshold, &root->root_keys_num, root->root_keys);
    } else if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "targets")) {
      ++idx;  // consume name token
      targets_found =
          parse_role(metadata_str, &idx, &root->targets_threshold, &root->targets_keys_num, root->targets_keys);
    } else {
      // ignore the other roles
      ++idx;                              //  consume name token
      idx = consume_recursive_json(idx);  // consume value
    }
  }

  *pos = idx;
  return root_found && targets_found;
}

bool uptane_parse_root_signed(const char *metadata_str, unsigned int *pos, uptane_root_t *out_root) {
  unsigned int idx = *pos;
  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return false;
  }
  int size = token_pool[idx].size;
  ++idx;  // consume object token

  for (int i = 0; i < size; ++i) {
    if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "_type")) {
      ++idx;  //  consume name token

      if (!JSON_STR_EQUAL(metadata_str, token_pool[idx], "Root")) {
        DEBUG_PRINTF("Wrong type of root metadata: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                     metadata_str + token_pool[idx].start);
        return false;
      }
      ++idx;
    } else if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "expires")) {
      ++idx;  //  consume name token

      uptane_time_t expires;
      if (!str2time(metadata_str + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), &expires)) {
        DEBUG_PRINTF("Invalid expiration date: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                     metadata_str + token_pool[idx].start);
        return false;
      }
      out_root->expires = expires;
      ++idx;
    } else if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "version")) {
      ++idx;  //  consume name token
      int32_t version_tmp;
      if (!dec2int(metadata_str + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), &version_tmp)) {
        DEBUG_PRINTF("Invalid version: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                     metadata_str + token_pool[idx].start);
        return false;
      }
      out_root->version = version_tmp;
      ++idx;  // consume value token
    } else if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "keys")) {
      ++idx;  //  consume name token
      if (!parse_keys(metadata_str, &idx)) {
        DEBUG_PRINTF("Failed to parse keys\n");

        // clean-up after parse_keys
        for (int j = 0; j < num_keys; j++) {
          free_crypto_key(keys[j]);
        }
        return false;
      }
    } else if (JSON_STR_EQUAL(metadata_str, token_pool[idx], "roles")) {
      ++idx;  //  consume name token
      if (!parse_roles(metadata_str, &idx, out_root)) {
        DEBUG_PRINTF("Failed to parse roles\n");
        return false;
      }
    } else {
      DEBUG_PRINTF("Unknown field in a root metadata: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                   metadata_str + token_pool[idx].start);
      ++idx;                              //  consume name token
      idx = consume_recursive_json(idx);  // consume value
    }
  }

  *pos = idx;
  return true;
}
