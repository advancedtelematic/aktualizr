#include "jsmn/jsmn.h"

extern jsmntok_t token_pool[];
extern const unsigned int token_pool_size;

unsigned int consume_recursive_json(unsigned int idx) {
  int size = token_pool[idx].size;
  switch (token_pool[idx].type) {
    case JSMN_OBJECT:
      ++idx;  // consume object token
      for (int i = 0; i < size; ++i) {
        ++idx;  // consume key token (string type)
        idx = consume_recursive_json(idx);
      }
      return idx;

    case JSMN_ARRAY:
      ++idx;  // consume array token
      for (int i = 0; i < size; ++i) {
        idx = consume_recursive_json(idx);
      }
      return idx;

    default:
      return idx + 1;  // just consume this token
  }
}
