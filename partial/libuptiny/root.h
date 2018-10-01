#ifndef LIBUPTINY_ROOT_H
#define LIBUPTINY_ROOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "state_api.h"
bool uptane_parse_root(const char *metadata, int16_t len, uptane_root_t *out_root);

#ifdef __cplusplus
}
#endif

#endif  // LIBUPTINY_ROOT_H
