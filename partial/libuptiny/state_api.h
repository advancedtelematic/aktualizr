#ifndef LIBUPTINY_STATE_API_H_
#define LIBUPTINY_STATE_API_H_

/* Implementation of state API functions is platform-specific and is out of
 * scope of libuptiny
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "crypto_api.h"
#include "uptane_time.h"

typedef enum { ROLE_ROOT, ROLE_TARGETS } uptane_role_t;

#define ROOT_MAX_KEYS 16

typedef struct {
  int32_t version;
  uptane_time_t expires;
  int32_t root_threshold;
  int32_t root_keys_num;
  crypto_key_t* root_keys[ROOT_MAX_KEYS];
  int32_t targets_threshold;
  int32_t targets_keys_num;
  crypto_key_t* targets_keys[ROOT_MAX_KEYS];
} uptane_root_t;

#define TARGETS_MAX_HASHES 4
#define TARGETS_MAX_NAME_LENGTH 63
/* Does not represent the whole targets metadata, only what's needed for this ECU */
typedef struct {
  int version;
  uptane_time_t expires;
  char name[TARGETS_MAX_NAME_LENGTH + 1];
  int hashes_num;
  crypto_hash_t hashes[TARGETS_MAX_HASHES];
  uint32_t length;
} uptane_targets_t;

typedef enum {
  ATTACK_NONE,
  ATTACK_ROOT_THRESHOLD,
  ATTACK_TARGETS_THRESHOLD,
  ATTACK_ROOT_VERSION,
  ATTACK_TARGETS_VERSION,
  ATTACK_ROOT_EXPIRED,
  ATTACK_TARGETS_EXPIRED,
  ATTACK_ROOT_LARGE,
  ATTACK_TARGETS_LARGE,
  ATTACK_IMAGE_HASH,
  ATTACK_IMAGE_LARGE,
} uptane_attack_t;

typedef struct {
  char firmware_name[TARGETS_MAX_NAME_LENGTH + 1];
  uptane_attack_t attack;
} uptane_installation_state_t;

uptane_root_t* state_get_root(void);
void state_set_root(const uptane_root_t* root);

uptane_targets_t* state_get_targets(void);
void state_set_targets(const uptane_targets_t* targets);

uptane_installation_state_t* state_get_installation_state(void);
void state_set_installation_state(char* firmware_name, uptane_attack_t attack);

const char* state_get_ecuid(void);
const char* state_get_hwid(void);

#ifdef __cplusplus
}
#endif

#endif  // LIBUPTINY_STATE_API_H_
