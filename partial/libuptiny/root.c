#include "root.h"
#include "common_data_api.h"
#include "crypto_api.h"
#include "debug.h"
#include "jsmn.h"
#include "json_common.h"
#include "root_signed.h"
#include "signatures.h"

bool uptane_parse_root(const char *metadata, size_t len, uptane_root_t *out_root) {
  int num_signatures = 0;
  unsigned int signatures_token = 0;
  int signed_begin = 0;
  int signed_end = 0;

  uptane_root_t *old_root = state_get_root();
  jsmn_parser parser;

  jsmn_init(&parser);

  if (jsmn_parse(&parser, metadata, len, token_pool, token_pool_size) < 0) {
    DEBUG_PRINTF("Preliminary parsing failed\n");
    return false;
  }

  if (token_pool[0].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return false;
  }

  int size = token_pool[0].size;
  unsigned idx = 1;  // consume object token
  for (int i = 0; i < size; ++i) {
    if (json_str_equal(metadata, idx, "signatures")) {
      ++idx;  // consume name token
      signatures_token = idx;
      num_signatures =
          uptane_parse_signatures(ROLE_ROOT, metadata, &idx, signature_pool, signature_pool_size, old_root);
      if (num_signatures <= 0) {
        DEBUG_PRINTF("Failed to parse signatures : %d\n", num_signatures);
        return false;
      }

    } else if (json_str_equal(metadata, idx, "signed")) {
      ++idx;  // consume name token
      signed_begin = token_pool[idx].start;
      signed_end = token_pool[idx].end;

      if (num_signatures <= 0) {
        DEBUG_PRINTF("No signatures to verify root metadata signed object\n");
        return false;
      }

      int num_valid_signatures = 0;
      for (int j = 0; j < num_signatures; j++) {
        crypto_verify_init(crypto_ctx_pool[j], &signature_pool[j]);
        crypto_verify_feed(crypto_ctx_pool[j], (const uint8_t *)metadata + signed_begin,
                           (size_t)(signed_end - signed_begin));
        if (crypto_verify_result(crypto_ctx_pool[j])) {
          ++num_valid_signatures;
        } else {
          DEBUG_PRINTF("Signature verification with old keys failed for key %d\n", j);
        }
      }

      if (num_valid_signatures < old_root->root_threshold) {
        DEBUG_PRINTF("Signature verification with old keys failed: only %d valid keys while threshold is %d\n",
                     num_valid_signatures, old_root->root_threshold);
        return false;
      }

      if (!uptane_parse_root_signed(metadata, &idx, out_root)) {
        DEBUG_PRINTF("Failed to parse signed part of root metadata\n");
        return false;
      }

      if (out_root->version < old_root->version) {
        DEBUG_PRINTF("Root metadata downgrade attempt\n");
        return false;
      }

      num_signatures = uptane_parse_signatures(ROLE_ROOT, metadata, &signatures_token, signature_pool,
                                               signature_pool_size, out_root);
      if (num_signatures <= 0) {
        DEBUG_PRINTF("Failed to parse signatures with new root: %d\n", num_signatures);
        return false;
      }

      num_valid_signatures = 0;
      for (int j = 0; j < num_signatures; j++) {
        crypto_verify_init(crypto_ctx_pool[j], &signature_pool[j]);
        crypto_verify_feed(crypto_ctx_pool[j], (const uint8_t *)metadata + signed_begin,
                           (size_t)(signed_end - signed_begin));
        if (crypto_verify_result(crypto_ctx_pool[j])) {
          ++num_valid_signatures;
        } else {
          DEBUG_PRINTF("Signature verification with new keys failed for key %d\n", j);
        }
      }

      if (num_valid_signatures < out_root->root_threshold) {
        DEBUG_PRINTF("Signature verification with new keys failed: only %d valid keys while threshold is %d\n",
                     num_valid_signatures, out_root->root_threshold);
        return false;
      }

      return true;  // success;

    } else {
      ++idx;  // consume name token
      DEBUG_PRINTF("Unknown field in key object: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                   metadata + token_pool[idx].start);
      idx = consume_recursive_json(idx);  // consume value
    }
  }
  DEBUG_PRINTF("No signed found\n");
  return false;
}
