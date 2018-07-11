#ifndef LIBUPTINY_STATE_API_H_
#define LIBUPTINY_STATE_API_H_

/* Implementation of state API functions is platform-specific and is out of
 * scope of libuptiny
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto.h"
#include "uptane_time.h"

typedef enum {ROLE_ROOT, ROLE_TARGETS} uptane_role_t;

#define ROOT_MAX_KEYS 16

typedef struct {
  int32_t version;
  uptane_time_t expires;
  int32_t root_threshold;
  int32_t root_keys_num;
  crypto_key_t *root_keys[ROOT_MAX_KEYS];
  int32_t targets_threshold;
  int32_t targets_keys_num;
  crypto_key_t *targets_keys[ROOT_MAX_KEYS];
} uptane_root_t;

/* Does not represent the whole targets metadata, only what's needed for this ECU */
typedef struct {
  int version;
  uptane_time_t expires;
  int hashes_num;
  crypto_hash_t **hashes;
  uint32_t length;
} uptane_target_t;

uptane_root_t* state_get_root(void);
void state_set_root(const uptane_root_t* root);

#ifdef __cplusplus
}
#endif

#endif //LIBUPTINY_STATE_API_H_
