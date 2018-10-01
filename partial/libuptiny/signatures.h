#ifndef LIBUPTINY_SIGNATURES_H
#define LIBUPTINY_SIGNATURES_H

#include "crypto_api.h"
#include "state_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int uptane_parse_signatures(uptane_role_t role, const char *signatures, int16_t *pos,
                            crypto_key_and_signature_t *output, unsigned int max_sigs, uptane_root_t *in_root);

#ifdef __cplusplus
}
#endif

#endif  // LIBUPTINY_SIGNATURES_H
