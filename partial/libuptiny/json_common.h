#ifndef LIBUPTINY_JSON_COMMON_H
#define LIBUPTINY_JSON_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

// consumes a token in token_pool indexed by idx recursively, returns index immediately after the consumed token
unsigned int consume_recursive_json(unsigned int idx);

#ifdef __cplusplus
}
#endif

#endif // LIBUPTINY_JSON_COMMON_H
