#ifndef LIBUPTINY_SIGNATURES_H
#define LIBUPTINY_SIGNATURES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto.h"
#include "state_api.h"

int uptane_parse_signatures(uptane_role_t role, const char *signatures, size_t len, crypto_key_and_signature_t *output, int max_sigs);

#ifdef __cplusplus
}
#endif

#endif // LIBUPTINY_SIGNATURES_H
