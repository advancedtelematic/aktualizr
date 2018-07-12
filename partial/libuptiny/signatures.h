#ifndef LIBUPTINY_SIGNATURES_H
#define LIBUPTINY_SIGNATURES_H

#include "crypto.h"
#include "state_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int uptane_parse_signatures(uptane_role_t role, const char *signatures, unsigned int *pos, crypto_key_and_signature_t *output, int max_sigs);

#ifdef __cplusplus
}
#endif

#endif // LIBUPTINY_SIGNATURES_H
