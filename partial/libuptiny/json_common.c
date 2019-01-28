#include "json_common.h"
#include "jsmn.h"

extern jsmntok_t token_pool[];
extern const int16_t token_pool_size;

int16_t consume_recursive_json(int16_t idx) {
  if (token_pool[idx].type != JSMN_OBJECT && token_pool[idx].type != JSMN_ARRAY) {
    return (int16_t)(idx + 1);
  }

  int end = token_pool[idx].end;
  int16_t i;

  for (i = (int16_t)(idx + 1); i < token_pool_size; ++i) {
    if (token_pool[i].start >= end) {
      break;
    }
  }
  return i;
}

bool json_str_equal(const char* json, int16_t idx, const char* value) {
  return ((long unsigned int)JSON_TOK_LEN(token_pool[idx]) == strlen(value) &&
          strncmp(value, (json) + (token_pool[idx]).start, strlen(value)) == 0);
}
