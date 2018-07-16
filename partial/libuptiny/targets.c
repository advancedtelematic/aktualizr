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
static jsmn_parser parser;         // jsmn parser
static int token_pos;              // current position in jsmn token array
static int ignored_top_token_pos;  // position of ignored object token in jsmn token array
static int signed_top_token_pos;   // position of "signed" object token in jsmn token array
static int targets_top_token_pos;  // position of "signed"."targets" object token in jsmn token array

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
static int num_signatures;  // number of signatures read
static int begin_signed;    // position in incoming message part where signed object begins
static int end_signed;      // position in incoming message part where signed object ends

int uptane_targets_get_num_signatures() { return num_signatures; }

int uptane_targets_get_begin_signed() { return begin_signed; }

int uptane_targets_get_end_signed() { return end_signed; }

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
}

typedef enum {
  PARSE_TARGET_ERROR,
  PARSE_TARGET_NOTFORME,
  PARSE_TARGET_FORME,
  PARSE_TARGET_WRONG_HW_ID,
} parse_target_result_t;

static inline parse_target_result_t parse_target(const char *message, unsigned int *pos, uptane_targets_t *target) {
  int idx = *pos;

  bool target_for_me = false;

  if (token_pool[idx].type != JSMN_STRING) {
    DEBUG_PRINTF("String expected\n");
    return PARSE_TARGET_ERROR;
  }

  if (JSON_TOK_LEN(token_pool[idx]) > TARGETS_MAX_NAME_LENGTH) {
    return PARSE_TARGET_ERROR;
  }

  memcpy(target->name, message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]));
  ++idx;  // consume target name token

  if (token_pool[idx].type != JSMN_OBJECT) {
    DEBUG_PRINTF("Object expected\n");
    return PARSE_TARGET_ERROR;
  }
  int size = token_pool[idx].size;
  ++idx;  // consume object token

  for (int i = 0; i < size; ++i) {
    if (JSON_STR_EQUAL(message, token_pool[idx], "custom")) {
      ++idx;  // consume name token
      if (token_pool[idx].type != JSMN_OBJECT) {
        DEBUG_PRINTF("Object expected\n");
        return PARSE_TARGET_ERROR;
      }
      int custom_size = token_pool[idx].size;
      ++idx;  // consume object token

      for (int j = 0; j < custom_size; ++j) {
        if (JSON_STR_EQUAL(message, token_pool[idx], "ecuIdentifiers")) {
          ++idx;  // consume name token
          if (token_pool[idx].type != JSMN_OBJECT) {
            DEBUG_PRINTF("Object expected\n");
            return PARSE_TARGET_ERROR;
          }
          int ecu_identifiers_size = token_pool[idx].size;
          ++idx;  // consume object token

          for (int k = 0; k < ecu_identifiers_size; ++k) {
            bool is_for_me =
                (strncmp(state_get_ecuid(), message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx])) == 0);
            ++idx;  // consume ECU ID token
            if (token_pool[idx].type != JSMN_OBJECT) {
              DEBUG_PRINTF("Object expected\n");
              return PARSE_TARGET_ERROR;
            }
            int hw_id_size = token_pool[idx].size;
            ++idx;  // consume object token

            for (int l = 0; l < hw_id_size; ++l) {
              if (JSON_STR_EQUAL(message, token_pool[idx], "hardwareId")) {
                ++idx;  // consume name token
                if (is_for_me &&
                    (strncmp(state_get_hwid(), message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx])) != 0)) {
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
    } else if (JSON_STR_EQUAL(message, token_pool[idx], "hashes")) {
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
            crypto_str_to_hashtype(message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]));
        ++idx;  // consume algorithm token
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

        if (JSON_TOK_LEN(token_pool[idx]) != crypto_get_hashlen(alg) * 2) {
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

    } else if (JSON_STR_EQUAL(message, token_pool[idx], "length")) {
      ++idx;  // consume name token
      int32_t length;
      if (!dec2int(message + token_pool[idx].start, JSON_TOK_LEN(token_pool[idx]), &length) || length < 0) {
        DEBUG_PRINTF("Invalid target length: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                     message + token_pool[idx].start);
        return PARSE_TARGET_ERROR;
      }
      target->length = length;
      ++idx;  // consume length token
    } else {
      DEBUG_PRINTF("Unknown field in a target: %.*s", (JSON_TOK_LEN(token_pool[idx])), message + token_pool[idx].start);
      ++idx;  // consume name token
      idx = consume_recursive_json(idx);
    }
  }

  *pos = idx;
  return (target_for_me) ? PARSE_TARGET_FORME : PARSE_TARGET_NOTFORME;
}

/*
 * @return number of consumed characters. The rest of the message should be presented to the parser on the next call
 */
int uptane_parse_targets_feed(const char *message, size_t len, uptane_targets_t *out_targets, uint16_t *result) {
  bool has_signed_begun = false;
  bool has_signed_ended = false;
  bool has_meta_ended = false;
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

  parser.pos = 0;
  parser.toknext = token_pos;
  jsmn_parse(&parser, message, len, token_pool, token_pool_size);

  has_meta_ended = (token_pos > 0 && token_pool[0].end >= 0);

  unsigned int idx;
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
        if (JSON_STR_EQUAL(message, token_pool[idx], "signatures")) {
          if (idx == parser.toknext - 1) {  // got "signatures", but not ': {'
            --idx;                          // return "signtatures" token to parse it on the next call
            break_parsing = true;
            break;
          }
          state = TARGETS_IN_SIGNATURES;
          ++idx;  // consume name token

          // consume everything before signatures and exit
          token_pool[idx].start = 0;  // next time signatures will start at the beginning of the message
          break_parsing = true;
          break;
        } else if (JSON_STR_EQUAL(message, token_pool[idx], "signed")) {
          if (num_signatures <= 0) {
            DEBUG_PRINTF("Signatures are not available for the signed part\n");
            state = TARGETS_IN_ERROR;
            break;
          }

          state = TARGETS_BEFORE_SIGNED;
          ++idx;  // consume name token
          signed_top_token_pos = idx;
          break;
        } else {
          prev_state = state;
          state = TARGETS_IN_IGNORED;
          ++idx;                        // consume name token
          ignored_top_token_pos = idx;  // remember value token number
          break;
        }
        break;

      case TARGETS_IN_IGNORED:
        DEBUG_PRINTF("TARGETS_IN_IGNORED\n");
        if (ignored_top_token_pos == idx) {  // the ignored object itself is obviously ignored
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
          num_signatures = uptane_parse_signatures(ROLE_TARGETS, message, &idx, signature_pool, signature_pool_size,
                                                   state_get_root());
          if (num_signatures <= 0) {
            DEBUG_PRINTF("Failed to parse signatures : %d\n", num_signatures);
            state = TARGETS_IN_ERROR;
            break;
          } else {
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

        if (num_signatures > crypto_ctx_pool_size) {
          num_signatures = crypto_ctx_pool_size;
        }

        begin_signed = token_pool[idx].start;
        has_signed_begun = true;  // local to the call

        state = TARGETS_IN_SIGNED;
        ++idx;  // consume object token
        break;

      case TARGETS_IN_SIGNED:
        if (token_pool[signed_top_token_pos].end >=
            0) {  // have read the whole "signed" object, but not necessarily in this token
          end_signed = token_pool[signed_top_token_pos].end;
          has_signed_ended = true;  // local to the call

          if (signed_elems_read >=
              token_pool[signed_top_token_pos].size) {  // should never be >, weaker condition for robustness
            state = TARGETS_IN_TOP;
            break;
          }
        }

        if (JSON_STR_EQUAL(message, token_pool[idx], "_type")) {
          if (idx == parser.toknext - 1) {  // got name, but not respective value
            --idx;                          // return name token to parse it on the next call
            break_parsing = true;
            break;
          }
          ++idx;  // consume name token
          ++signed_elems_read;
          if (!JSON_STR_EQUAL(message, token_pool[idx], "Targets")) {
            DEBUG_PRINTF("Wrong type of targets metadata: \"%.*s\"\n", JSON_TOK_LEN(token_pool[idx]),
                         message + token_pool[idx].start);
            state = TARGETS_IN_ERROR;
            break;
          }
          ++idx;  // consume value token
        } else if (JSON_STR_EQUAL(message, token_pool[idx], "expires")) {
          if (idx == parser.toknext - 1) {  // got name, but not respective value
            --idx;                          // return name token to parse it on the next call
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
        } else if (JSON_STR_EQUAL(message, token_pool[idx], "version")) {
          if (idx == parser.toknext - 1) {  // got name, but not respective value
            --idx;                          // return name token to parse it on the next call
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
          out_targets->version = version_tmp;
          ++idx;  // consume value token
        } else if (JSON_STR_EQUAL(message, token_pool[idx], "targets")) {
          if (idx == parser.toknext - 1) {  // got "targets", but not ': {'
            --idx;                          // return "targets" token to parse it on the next call
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
        unsigned int target_elem_idx = idx;
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
          }

          idx = target_elem_idx;  // target_elem_idx has been advanced by parse_target to point to the next target
          ++targets_elems_read;
        } else {
          idx = target_elem_idx - 1;  // rewind to the point before target name
          break_parsing = true;
          break;
        }

        break;

      default:
        DEBUG_PRINTF("Unexpected state");
        state = TARGETS_IN_ERROR;
        break;
    }
  }

  /* Second check after processing tokens */
  if (state == TARGETS_IN_ERROR) {
    *result = RESULT_ERROR;
    return -1;
  }

  /* Processed the whole metadata, return result */
  *result = 0x0000;
  if (has_meta_ended) {
    if (!target_found) {
      *result |= RESULT_END_NOT_FOUND;
    } else {
      *result |= RESULT_END_FOUND;
    }
  }
  if (has_signed_begun) {
    *result |= RESULT_BEGIN_SIGNED;
  }
  if (has_signed_ended) {
    *result |= RESULT_END_SIGNED;
  }

  /* Metadata hasn't ended, common case */
  if (idx > token_pos) {
    token_pos = idx;  // start on the current idx next time
    if (token_pool[idx - 1].end > 0) {
      // last token was primitive or string. If it was a string, we've got a closing quote to consume. In any case,
      // there can be 'non-tokens' like ':', ',', '}', ']' that are already processed and shouldn't be given to the
      // parser again
      int res = token_pool[idx - 1].end;
      if (token_pool[idx - 1].type == JSMN_STRING) {
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
      return token_pool[idx - 1].start + 1;
    }
  } else {
    return 0;
  }
}
