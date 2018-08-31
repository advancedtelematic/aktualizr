#include "signatures.h"
#include "base64.h"
#include "crypto_common.h"
#include "debug.h"
#include "jsmn.h"
#include "json_common.h"
#include "utils.h"

extern jsmntok_t token_pool[];
extern const unsigned int token_pool_size;

crypto_key_t **meta_keys;
int meta_keys_num;

static inline int parse_sig(const char *json_sig, unsigned int *pos, crypto_key_and_signature_t *sig) {
  unsigned int idx = *pos;

  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return -1;
  }
  int size = token_pool[idx].size;
  ++idx;  // consume object token

  bool key_found = false;
  bool sig_found = false;
  uint8_t sig_buf[CRYPTO_MAX_SIGNATURE_LEN + 2];  // '+2' because base64 operates in 3 byte granularity
  for (int i = 0; i < size; ++i) {
    if (JSON_STR_EQUAL(json_sig, token_pool[idx], "keyid")) {
      ++idx;  //  consume name token
      if (token_pool[idx].type != JSMN_STRING) {
        DEBUG_PRINTF("Key ID is not a string\n");
        idx = consume_recursive_json(idx);
      } else {
        const crypto_key_t *key = find_key(json_sig + token_pool[idx].start,
                                           token_pool[idx].end - token_pool[idx].start, meta_keys, meta_keys_num);
        if (key) {
          sig->key = key;
          key_found = true;
        }
        ++idx;  // consume key
      }
    } else if (JSON_STR_EQUAL(json_sig, token_pool[idx], "method")) {
      // method is ignored for now
      ++idx;                              //  consume name token
      idx = consume_recursive_json(idx);  // consume value
    } else if (JSON_STR_EQUAL(json_sig, token_pool[idx], "sig")) {
      ++idx;  //  consume name token
      if (token_pool[idx].type != JSMN_STRING) {
        DEBUG_PRINTF("Signature is not a string\n");
        idx = consume_recursive_json(idx);
      } else {
        int b64_len = JSON_TOK_LEN(token_pool[idx]);
        if (b64_len > 0 && BASE64_DECODED_BUF_SIZE(b64_len) <= CRYPTO_MAX_SIGNATURE_LEN + 2) {
          if (CRYPTO_MAX_SIGNATURE_LEN ==
              base64_decode(json_sig + token_pool[idx].start, (unsigned int)b64_len, sig_buf)) {
            memcpy(sig->sig, sig_buf, CRYPTO_MAX_SIGNATURE_LEN);
            sig_found = true;
          } else {
            DEBUG_PRINTF("Unexpected signature size\n");
          }
        } else {
          DEBUG_PRINTF("Signature is too large\n");
        }
        ++idx;  // consume signature
      }
    } else {
      DEBUG_PRINTF("Unknown field in a signature: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                   json_sig + token_pool[idx].start);
      ++idx;                              //  consume name token
      idx = consume_recursive_json(idx);  // consume value
    }
  }

  *pos = idx;
  return (sig_found && key_found);
}

int uptane_parse_signatures(uptane_role_t role, const char *signatures, unsigned int *pos,
                            crypto_key_and_signature_t *output, unsigned int max_sigs, uptane_root_t *in_root) {
  if (role == ROLE_ROOT) {
    meta_keys = in_root->root_keys;
    meta_keys_num = in_root->root_keys_num;
  } else {  // ROLE_TARGETS
    meta_keys = in_root->targets_keys;
    meta_keys_num = in_root->targets_keys_num;
  }

  unsigned int token_idx = *pos;
  if (token_pool[token_idx].type != JSMN_ARRAY) {
    DEBUG_PRINTF("Array expected\n");
    return -1;
  }
  int array_size = token_pool[token_idx].size;
  ++token_idx;  // Consume array token

  unsigned int sigs_read = 0;

  for (int i = 0; i < array_size; ++i) {
    if (sigs_read >= max_sigs) {
      *pos = token_idx;
      DEBUG_PRINTF("Too many, signatures, only %d are used\n", sigs_read);
      return (int)sigs_read;
    }

    DEBUG_PRINTF("Parse signature at %d\n", token_idx);
    int res = parse_sig(signatures, &token_idx, output + sigs_read);
    if (res < 0) {
      return -1;
    } else if (res != 0) {
      ++sigs_read;
    }
  }
  *pos = token_idx;
  return (int)sigs_read;
}
