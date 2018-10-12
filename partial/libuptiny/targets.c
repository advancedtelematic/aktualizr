#include "targets.h"

#include "common_data_api.h"
#include "crypto_api.h"
#include "debug.h"
#include "json_common.h"
#include "signatures.h"
#include "state_api.h"
#include "utils.h"

/*
 * State variables, initialized in uptane_parse_targets_init()
 */
static jsmn_parser parser;             // jsmn parser
static int16_t token_pos;              // current position in jsmn token array
static int16_t ignored_top_token_pos;  // position of ignored object token in jsmn token array
static int16_t signed_top_token_pos;   // position of "signed" object token in jsmn token array
static int16_t targets_top_token_pos;  // position of "signed"."targets" object token in jsmn token array

typedef enum {
  TARGETS_BEGIN,          // initial state
  TARGETS_IN_TOP,         // pointer in the top object (above "signatures" and "signed")
  TARGETS_IN_SIGNATURES,  // pointer in the "signatures" object
  TARGETS_BEFORE_SIGNED,  // pointer consumed "signed" object name and waits for the object
  TARGETS_IN_SIGNED,      // pointer in the "signed" object
  TARGETS_IN_IGNORED,     // pointer inside ignored object/array
  TARGETS_IN_TARGETS,     // pointer inside "signed"."targets" object
  TARGETS_IN_ERROR        // encountered an error, sink state
} parsing_state_t;

static parsing_state_t state;
static parsing_state_t prev_state;  // state to return to from TARGETS_IN_IGNORED

static bool target_found;  // found a target for this ECU

static int signed_elems_read;   // number of elements of "signed" object already read
static int targets_elems_read;  // number of elements of "signed".targets object already read

/*
 * Values to be returned via getters
 */
static unsigned int num_signatures;  // number of signatures read
static int16_t begin_signed;         // position in incoming message part where signed object begins
static int16_t end_signed;           // position in incoming message part where signed object ends

bool in_signed;  // if the signature verification is in progress. Different from 'state == TARGETS_IN_SIGNED' in that
                 // the state machine operates independently of signature verification and the two values can be out of
                 // sync for a short time.
static int16_t tail_length;  // number of bytes fed, but not consumed on the last call. Used for signature verification

void uptane_parse_targets_init(void) {
  jsmn_init(&parser);
  token_pos = 0;
  ignored_top_token_pos = 0;
  signed_top_token_pos = -1;
  targets_top_token_pos = -1;
  targets_top_token_pos = -1;
  target_found = false;
  signed_elems_read = 0;
  targets_elems_read = 0;
  num_signatures = 0;
  state = TARGETS_BEGIN;

  begin_signed = end_signed = -1;

  in_signed = false;
  tail_length = 0;
}

typedef enum {
  PARSE_TARGET_ERROR,
  PARSE_TARGET_NOTFORME,
  PARSE_TARGET_FORME,
  PARSE_TARGET_WRONG_HW_ID,
} parse_target_result_t;

static inline parse_target_result_t parse_target(const char *message, int16_t *pos, uptane_targets_t *target) {
  int16_t idx = *pos;

  bool target_for_me = false;

  if (token_pool[idx].type != JSMN_STRING) {
    DEBUG_PRINTF("String expected\n");
    return PARSE_TARGET_ERROR;
  }

  int target_name_length = JSON_TOK_LEN(token_pool[idx]);
  if (target_name_length > TARGETS_MAX_NAME_LENGTH || target_name_length <= 0) {
    return PARSE_TARGET_ERROR;
  }

  memcpy(target->name, message + token_pool[idx].start, (size_t)target_name_length);
  ++idx;  // consume target name token

  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return PARSE_TARGET_ERROR;
  }
  int size = token_pool[idx].size;
  ++idx;  // consume object token

  for (int i = 0; i < size; ++i) {
    if (json_str_equal(message, idx, "custom")) {
      ++idx;  // consume name token
      if (token_pool[idx].type != JSMN_OBJECT) {
        DEBUG_PRINTF("Object expected\n");
        return PARSE_TARGET_ERROR;
      }
      int custom_size = token_pool[idx].size;
      ++idx;  // consume object token

      for (int j = 0; j < custom_size; ++j) {
        if (json_str_equal(message, idx, "ecuIdentifiers")) {
          ++idx;  // consume name token
          if (token_pool[idx].type != JSMN_OBJECT) {
            DEBUG_PRINTF("Object expected\n");
            return PARSE_TARGET_ERROR;
          }
          int ecu_identifiers_size = token_pool[idx].size;
          ++idx;  // consume object token

          for (int k = 0; k < ecu_identifiers_size; ++k) {
            bool is_for_me = json_str_equal(message, idx, state_get_ecuid());
            ++idx;  // consume ECU ID token
            if (token_pool[idx].type != JSMN_OBJECT) {
              DEBUG_PRINTF("Object expected\n");
              return PARSE_TARGET_ERROR;
            }
            int hw_id_size = token_pool[idx].size;
            ++idx;  // consume object token

            for (int l = 0; l < hw_id_size; ++l) {
              if (json_str_equal(message, idx, "hardwareId")) {
                ++idx;  // consume name token
                if (is_for_me && !json_str_equal(message, idx, state_get_hwid())) {
                  DEBUG_PRINTF("Invalid hardware identifier: %.*s\n", JSON_TOK_LEN(token_pool[idx]),
                               message + token_pool[idx].start);
                  return PARSE_TARGET_WRONG_HW_ID;
                }
                ++idx;  // consume HW ID token
              } else {
                DEBUG_PRINTF("Unknown field in a ecuIdentifier's object: %.*s\n", JSON_TOK_LEN(token_pool[idx]),
                             message + token_pool[idx].start);
                ++idx;  // consume name token
                idx = consume_recursive_json(idx);
              }
            }
            if (is_for_me) {
              target_for_me = true;
            }
          }

        } else {
          DEBUG_PRINTF("Unknown field in a target's custom: %.*s\n", JSON_TOK_LEN(token_pool[idx]),
                       message + token_pool[idx].start);
          ++idx;  // consume name token
          idx = consume_recursive_json(idx);
        }
      }
    } else if (json_str_equal(message, idx, "hashes")) {
      ++idx;  // consume name token
      if (token_pool[idx].type != JSMN_OBJECT) {
        DEBUG_PRINTF("Object expected\n");
        return PARSE_TARGET_ERROR;
      }
      int hashes_size = token_pool[idx].size;
      ++idx;  // consume object token

      int hash_idx = 0;
      for (int j = 0; j < hashes_size; ++j) {
        crypto_hash_algorithm_t alg =
            crypto_str_to_hashtype(message + token_pool[idx].start,
                                   (size_t)JSON_TOK_LEN(token_pool[idx]));  // trust jsmn_parse to keep lengths >= 0
        ++idx;                                                              // consume algorithm token
        if (alg == CRYPTO_HASH_UNKNOWN) {
          DEBUG_PRINTF("Unknown hash algorithm: %.*s\n", JSON_TOK_LEN(token_pool[idx - 1]),
                       message + token_pool[idx - 1].start);
          idx = consume_recursive_json(idx);
          continue;
        }

        if (hash_idx >= TARGETS_MAX_HASHES) {
          DEBUG_PRINTF("Too many hashes\n");
          continue;
        }

        if ((size_t)JSON_TOK_LEN(token_pool[idx]) != crypto_get_hashlen(alg) * 2) {
          DEBUG_PRINTF("Invalid hash length: %d when %d expected\n", JSON_TOK_LEN(token_pool[idx]),
                       crypto_get_hashlen(alg));
          return PARSE_TARGET_ERROR;
        }

        target->hashes[hash_idx].alg = alg;
        if (!hex2bin(message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), target->hashes[hash_idx].hash)) {
          DEBUG_PRINTF("Failed to parse hash\n");
          return PARSE_TARGET_ERROR;
        }
        ++idx;  // consume hash token
        ++hash_idx;
      }
      target->hashes_num = hash_idx;

    } else if (json_str_equal(message, idx, "length")) {
      ++idx;  // consume name token
      int32_t length;
      if (!dec2int(message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), &length) || length < 0) {
        DEBUG_PRINTF("Invalid target length: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                     message + token_pool[idx].start);
        return PARSE_TARGET_ERROR;
      }
      target->length = (uint32_t)length;
      ++idx;  // consume length token
    } else {
      DEBUG_PRINTF("Unknown field in a target: %.*s\n", (JSON_TOK_LEN(token_pool[idx])),
                   message + token_pool[idx].start);
      ++idx;  // consume name token
      idx = consume_recursive_json(idx);
    }
  }

  *pos = idx;
  return (target_for_me) ? PARSE_TARGET_FORME : PARSE_TARGET_NOTFORME;
}

// calculates number of characters consumed by uptane_parse_targets_feed when some tokens were consumed
//  idx > 0 in this case
static inline int16_t consumed_chars_newtoken(const char *message, int16_t len, int16_t idx) {
  if (token_pool[idx - 1].end > 0) {
    // last token was primitive or string. If it was a string, we've got a closing quote to consume. In any case,
    // there can be 'non-tokens' like ':', ',', '}', ']' that are already processed and shouldn't be given to the
    // parser again
    int16_t res = (size_t)token_pool[idx - 1].end;
    if (token_pool[idx - 1].type == JSMN_STRING && message[res] == '"') {
      ++res;
    }
    for (; res < len; ++res) {
      switch (message[res]) {
        case ':':
        case ',':
        case '}':
        case ']':
          break;
        default:
          return res;
      }
    }
    return res;
  } else {
    // NOLINTNEXTLINE(misc-misplaced-widening-cast)
    return (int16_t)(token_pool[idx - 1].start + 1);  // start should not be negative if jsmn_parse works correctly
  }
}

// calculates the number of characters consumed by uptane_parse_targets_feed when no tokens were consumed (but some
// non-token characters may need to be eaten anyway)
static inline int16_t consumed_chars_nonewtoken(const char *message, int16_t len) {
  int16_t res = 0;
  if (token_pos > 0) {
    for (; res < len; ++res) {
      switch (message[res]) {
        case ':':
        case ',':
        case '}':
        case ']':
          break;
        default:
          return res;
      }
    }
  }
  return res;
}

// prepare jsmn parser to a new jsmn_parse round. It might be in a broken state because some characters were fed to
// jsmn_parse, but not really consumed by uptane_parse_targets_feed
static void prepare_primary_parser(void) {
  parser.pos = 0;
  parser.toknext = token_pos;

  // rewind toksuper. toksuper can be either container (JSMN_OBJECT/JSMN_ARRAY) or a string.
  parser.toksuper = -1;
  //   If the last token is an identifier string, i.e. a string whose direct parent is an object, set it as toksuper
  if (token_pos > 0) {
    if (token_pool[token_pos - 1].type == JSMN_STRING &&
        token_pool[token_pool[token_pos - 1].parent].type == JSMN_OBJECT) {
      parser.toksuper = (int16_t)(token_pos - 1);  // token_pos >= 1
    } else {
      int16_t i = (int16_t)(token_pos - 1);  // token_pos >= 1
      while (i >= 0 &&
             ((token_pool[i].type != JSMN_OBJECT && token_pool[i].type != JSMN_ARRAY) || token_pool[i].end >= 0)) {
        i = token_pool[i].parent;
      }
      parser.toksuper = i;
    }
  }
}

/*
 * @return number of consumed characters. The rest of the message should be presented to the parser on the next call
 */
int uptane_parse_targets_feed(const char *message, int16_t len, uptane_targets_t *out_targets, uint16_t *result) {
  bool has_signed_begun = false;
  bool has_signed_ended = false;
  bool break_parsing = false;

  // 1. No new tokens => return 0
  // 2. N new tokens =>
  //   2a. In top object => consume name token; if "signed" or "signatures" proceed to respective branch. Otherwise
  //   proceed to "consume recursive"
  //   2b. In "consume recursive" => remember top token number and consume subsequent tokens until its end is
  //   non-negative
  //   2c. In "signatures" => don't consume any characters until top object's "end" is non-negative, then parse
  //   signatures
  //   2d. In "signed" => [on entrance start calculating signature] if one of "_type", "expires", "version" - wait for
  //   the value and consume both, if "targets", proceed to "targets", else proceed to "consume recursive"
  //   2e. In "targets" => wait for key:value pair to be complete (both have "end" non-negative), then call parse_target
  //   and consume both

  // If the state is ERROR, don't try to parse the feed
  if (state == TARGETS_IN_ERROR) {
    *result = RESULT_ERROR;
    return -1;
  }

  // initialize primary parser
  prepare_primary_parser();

  jsmn_parse(&parser, message, len, token_pool, token_pool_size);

  int16_t idx;
  for (idx = token_pos; idx < parser.toknext && !break_parsing && state != TARGETS_IN_ERROR;) {
    switch (state) {
      case TARGETS_BEGIN:
        if (token_pool[idx].type != JSMN_OBJECT) {
          DEBUG_PRINTF("Object expected\n");
          state = TARGETS_IN_ERROR;
        } else {
          state = TARGETS_IN_TOP;
          ++idx;  // consume object token
        }
        break;
      case TARGETS_IN_TOP:
        if (json_str_equal(message, idx, "signatures")) {
          if (idx == parser.toknext - 1 || token_pool[idx + 1].end < 0) {  // got "signatures", but not actual object
            // remove partially parsed object from the container
            --token_pool[0].size;
            break_parsing = true;
            break;
          }
          state = TARGETS_IN_SIGNATURES;
          ++idx;  // consume name token

          break;
        } else if (json_str_equal(message, idx, "signed")) {
          if (num_signatures == 0) {
            DEBUG_PRINTF("Signatures are not available for the signed part\n");
            state = TARGETS_IN_ERROR;
            break;
          }

          state = TARGETS_BEFORE_SIGNED;
          ++idx;  // consume name token
          break;
        } else {
          prev_state = state;
          state = TARGETS_IN_IGNORED;
          ++idx;                             // consume name token
          ignored_top_token_pos = (int)idx;  // remember value token number
          break;
        }
        break;

      case TARGETS_IN_IGNORED:
        if (ignored_top_token_pos == (int)idx) {  // the ignored object itself is obviously ignored
          ++idx;
        } else if (token_pool[ignored_top_token_pos].end < 0) {  // not yet reached the end of the top object
          ++idx;  // *.end will never change in this call, so consume all the tokens read
        } else {
          while (idx < parser.toknext &&
                 token_pool[idx].start < token_pool[ignored_top_token_pos].end) {  // consume the rest of ignored object
                                                                                   // within this function call. On the
                                                                                   // next call we should be in the
                                                                                   // previous state
            ++idx;  // token is under the ignored one, consume
          }

          state = prev_state;  // don't consume the token, it will be done by the next state
        }
        break;

      case TARGETS_IN_SIGNATURES:
        if (token_pool[idx].end < 0) {
          // everything before "signatures" has already been consumed in 'case TARGETS_IN_TOP'
          break_parsing = true;
          break;
        } else {
          int parse_res = uptane_parse_signatures(ROLE_TARGETS, message, &idx, signature_pool, signature_pool_size,
                                                  state_get_root());
          if (parse_res <= 0) {
            DEBUG_PRINTF("Failed to parse signatures : %d\n", num_signatures);
            state = TARGETS_IN_ERROR;
            break;
          } else {
            num_signatures = (unsigned int)parse_res;
            state = TARGETS_IN_TOP;
          }
        }
        break;
      case TARGETS_BEFORE_SIGNED:
        if (token_pool[idx].type != JSMN_OBJECT) {
          DEBUG_PRINTF("Object expected\n");
          state = TARGETS_IN_ERROR;
          break;
        }

        signed_top_token_pos = idx;
        if (num_signatures > crypto_ctx_pool_size) {
          num_signatures = crypto_ctx_pool_size;
        }

        begin_signed = token_pool[idx].start;
        has_signed_begun = true;  // local to the call

        state = TARGETS_IN_SIGNED;
        ++idx;  // consume object token
        break;

      case TARGETS_IN_SIGNED:
        if (token_pool[signed_top_token_pos].end >= 0 &&
            signed_elems_read >=
                token_pool[signed_top_token_pos].size) {  // should never be >, weaker condition for robustness
          state = TARGETS_IN_TOP;
          break;
        }

        if (json_str_equal(message, idx, "_type")) {
          if (idx == parser.toknext - 1) {  // got name, but not respective value
            // remove partially parsed object from the container
            --token_pool[signed_top_token_pos].size;
            break_parsing = true;
            break;
          }
          ++idx;  // consume name token
          ++signed_elems_read;
          if (!json_str_equal(message, idx, "Targets")) {
            DEBUG_PRINTF("Wrong type of targets metadata: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                         message + token_pool[idx].start);
            state = TARGETS_IN_ERROR;
            break;
          }
          ++idx;  // consume value token
        } else if (json_str_equal(message, idx, "expires")) {
          if (idx == parser.toknext - 1) {  // got name, but not respective value
            // remove partially parsed object from the container
            --token_pool[signed_top_token_pos].size;
            break_parsing = true;
            break;
          }
          ++signed_elems_read;
          ++idx;  // consume name token

          uptane_time_t expires;
          if (!str2time(message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), &expires)) {
            DEBUG_PRINTF("Invalid expiration date: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                         message + token_pool[idx].start);
            state = TARGETS_IN_ERROR;
            break;
          } else {
            out_targets->expires = expires;
          }
          ++idx;  // consume value token
        } else if (json_str_equal(message, idx, "version")) {
          if (idx == parser.toknext - 1) {  // got name, but not respective value
            // remove partially parsed object from the container
            --token_pool[signed_top_token_pos].size;
            break_parsing = true;
            break;
          }
          ++signed_elems_read;
          ++idx;  // consume name token

          int32_t version_tmp;
          if (!dec2int(message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), &version_tmp)) {
            DEBUG_PRINTF("Invalid version: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]), message + token_pool[idx].start);
            state = TARGETS_IN_ERROR;
            break;
          }

          if (version_tmp < state_get_targets()->version) {
            state = TARGETS_IN_ERROR;
            *result = RESULT_VERSION_FAILED;
            return -1;
          }
          out_targets->version = version_tmp;
          ++idx;  // consume value token
        } else if (json_str_equal(message, idx, "targets")) {
          if (idx == parser.toknext - 1) {  // got "targets", but not ': {'
            // remove partially parsed object from the container
            --token_pool[signed_top_token_pos].size;
            break_parsing = true;
            break;
          }
          state = TARGETS_IN_TARGETS;
          ++idx;  // consume name token
          targets_top_token_pos = idx;

          if (token_pool[idx].type != JSMN_OBJECT) {
            DEBUG_PRINTF("Object expected\n");
            state = TARGETS_IN_ERROR;
          }
          ++idx;  // consume object token
          break;
        } else {
          prev_state = state;
          state = TARGETS_IN_IGNORED;
          ++idx;  // consume name token
          ++signed_elems_read;
          ignored_top_token_pos = idx;  // remember value token number
          break;
        }
        break;

      case TARGETS_IN_TARGETS:
        if (token_pool[targets_top_token_pos].end >= 0) {
          if (targets_elems_read >=
              token_pool[targets_top_token_pos].size) {  // should never be >, weaker condition for robustness
            ++signed_elems_read;
            state = TARGETS_IN_SIGNED;
            break;
          }
        }

        if (token_pool[idx].type != JSMN_STRING) {
          DEBUG_PRINTF("String expected\n");
          state = TARGETS_IN_ERROR;
          break;
        }
        int16_t target_elem_idx = idx;
        ++idx;  // consume target name token

        if (idx < parser.toknext && token_pool[idx].end > 0) {  // target object parsed completely
          static uptane_targets_t tmp_target;
          parse_target_result_t res = parse_target(message, &target_elem_idx, &tmp_target);
          switch (res) {
            case PARSE_TARGET_ERROR:
              DEBUG_PRINTF("Error parsing target\n");
              state = TARGETS_IN_ERROR;
              break;

            case PARSE_TARGET_NOTFORME:
              break;

            case PARSE_TARGET_FORME:
              if (target_found) {
                DEBUG_PRINTF("Multiple targets for this ECU\n");
                state = TARGETS_IN_ERROR;
                break;
              }

              target_found = true;
              out_targets->hashes_num = tmp_target.hashes_num;
              memcpy(&out_targets->name, &tmp_target.name, sizeof(tmp_target.name));
              memcpy(&out_targets->hashes, &tmp_target.hashes, sizeof(tmp_target.hashes));
              out_targets->length = tmp_target.length;
              break;

            case PARSE_TARGET_WRONG_HW_ID:
              state = TARGETS_IN_ERROR;
              *result = RESULT_WRONG_HW_ID;
              return -1;

            default:
              state = TARGETS_IN_ERROR;
              *result = RESULT_ERROR;
              return -1;
          }

          idx = target_elem_idx;  // target_elem_idx has been advanced by parse_target to point to the next target
          ++targets_elems_read;
        } else {
          idx = (int16_t)(target_elem_idx - 1);  // rewind to the point before target name
          --token_pool[targets_top_token_pos].size;
          break_parsing = true;
          break;
        }

        break;

      default:
        DEBUG_PRINTF("Unexpected state\n");
        state = TARGETS_IN_ERROR;
        break;
    }
  }

  /* Second check after processing tokens */
  if (state == TARGETS_IN_ERROR) {
    *result = RESULT_ERROR;
    return -1;
  }

  if ((signed_top_token_pos >= 0) && (end_signed < 0) &&
      token_pool[signed_top_token_pos].end >= 0) {  // have read the whole "signed" object
    end_signed = token_pool[signed_top_token_pos].end;
    has_signed_ended = true;  // local to the call
  }

  /* signature verification */
  if (has_signed_begun) {
    in_signed = true;
    for (unsigned int i = 0; i < num_signatures; i++) {
      crypto_verify_init(crypto_ctx_pool[i], &signature_pool[i]);
    }
  }

  if (in_signed) {
    int16_t first_signed;
    int16_t last_signed;
    if (has_signed_begun) {
      first_signed = begin_signed;  // once has_signed_begun is set, begin_signed >= 0
    } else {
      first_signed = tail_length;
    }
    if (has_signed_ended) {
      last_signed = end_signed;  // once has_signed_begun is set, begin_signed >= 0
    } else {
      last_signed = len;
    }

    for (unsigned int i = 0; i < num_signatures; i++) {
      crypto_verify_feed(crypto_ctx_pool[i], (const uint8_t *)message + first_signed,
                         (size_t)(last_signed - first_signed));
    }
  }

  if (has_signed_ended) {
    in_signed = false;
    int num_valid_signatures = 0;
    for (unsigned int i = 0; i < num_signatures; i++) {
      if (crypto_verify_result(crypto_ctx_pool[i])) {
        ++num_valid_signatures;
      } else {
        DEBUG_PRINTF("Signature verification failed for signature %d\n", i);
      }
    }

    if (num_valid_signatures < state_get_root()->targets_threshold) {
      DEBUG_PRINTF("Signature verification failed: only %d signatures are valid with threshold of %d\n",
                   num_valid_signatures, state_get_root()->targets_threshold);
      state = TARGETS_IN_ERROR;
      *result = RESULT_SIGNATURES_FAILED;
      return -1;
    }
  }

  if ((idx > 0 && token_pool[0].end >= 0)) {
    /* Processed the whole metadata, return result */
    if (!target_found) {
      *result = RESULT_END_NOT_FOUND;
    } else {
      *result = RESULT_END_FOUND;
    }
  } else {
    *result = RESULT_IN_PROGRESS;
  }

  int16_t ret;
  /* Advance token and character positions */
  if (idx > token_pos) {
    ret = consumed_chars_newtoken(message, len, idx);
    token_pos = idx;  // start on the current idx next time
  } else {
    ret = consumed_chars_nonewtoken(message, len);
  }

  tail_length = (int16_t)(len - ret);
  return (int)ret;
}
