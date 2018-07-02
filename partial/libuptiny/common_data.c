#include "jsmn/jsmn.h"

#define TOKEN_POOL_SIZE 100

jsmntok_t token_pool[TOKEN_POOL_SIZE];
const unsigned int token_pool_size = TOKEN_POOL_SIZE;
