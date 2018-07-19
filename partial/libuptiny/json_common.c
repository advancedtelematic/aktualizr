#include "jsmn/jsmn.h"

extern jsmntok_t token_pool[];
extern const unsigned int token_pool_size;

unsigned int consume_recursive_json(unsigned int idx) {
  if (token_pool[idx].type != JSMN_OBJECT && token_pool[idx].type != JSMN_ARRAY) {
    return idx + 1;
  }

  int end = token_pool[idx].end;
  int i;

  for (i = idx + 1; i < token_pool_size; ++i) {
    if (token_pool[i].start >= end) {
      break;
    }
  }
  return i;
}
