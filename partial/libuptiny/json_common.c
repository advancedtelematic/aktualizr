#include "jsmn.h"
#include "json_common.h"

extern jsmntok_t token_pool[];
extern const unsigned int token_pool_size;

unsigned int consume_recursive_json(unsigned int idx) {
  if (token_pool[idx].type != JSMN_OBJECT && token_pool[idx].type != JSMN_ARRAY) {
    return idx + 1;
  }

  int end = token_pool[idx].end;
  unsigned int i;

  for (i = idx + 1; i < token_pool_size; ++i) {
    if (token_pool[i].start >= end) {
      break;
    }
  }
  return i;
}

bool json_str_equal(const char* json, unsigned int idx, const char* value) {
  return ((long unsigned int)JSON_TOK_LEN(token_pool[idx]) == strlen(value) && \
   strncmp(value, (json) + (token_pool[idx]).start, strlen(value)) == 0);
}
