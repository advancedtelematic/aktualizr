#include "firmware.h"
#include "common_data_api.h"
#include "crypto_api.h"
#include "state_api.h"

#include <string.h>

const crypto_hash_t *expected_hash;
bool firmware_updated = true;

bool uptane_firmware_updated(void) {
  bool res = firmware_updated;
  firmware_updated = false;
  return res;
}

bool uptane_verify_firmware_init(void) {
  const uptane_targets_t *targets = state_get_targets();

  if (!targets) {
    return false;
  }
  const uptane_installation_state_t *state = state_get_installation_state();

  expected_hash = NULL;
  crypto_hash_algorithm_t alg = state_get_supported_hash();
  for (int i = 0; i < targets->hashes_num; ++i) {
    if (targets->hashes[i].alg == alg) {
      expected_hash = &targets->hashes[i];
    }
  }

  if (expected_hash == NULL) {
    return false;
  }

  if (state) {
    if (!memcmp(expected_hash->hash, state->firmware_hash.hash, crypto_get_hashlen(alg))) {
      return false;  // expected hash matches what is already installed, no update
    }
  }
  crypto_hash_init(&hash_context);
  return true;
}

void uptane_verify_firmware_feed(const uint8_t *data, size_t len) { crypto_hash_feed(&hash_context, data, len); }

bool uptane_verify_firmware_finalize(void) {
  crypto_hash_t computed_hash;
  crypto_hash_result(&hash_context, &computed_hash);
  return !memcmp(computed_hash.hash, expected_hash->hash, crypto_get_hashlen(expected_hash->alg));
}

void uptane_firmware_confirm(void) {
  const uptane_targets_t *targets = state_get_targets();

  if (!targets) {
    return;
  }

  uptane_installation_state_t new_state;
  strncpy(new_state.firmware_name, targets->name, TARGETS_MAX_NAME_LENGTH + 1);
  new_state.firmware_hash = *expected_hash;
  new_state.firmware_length = targets->length;
  new_state.attack = ATTACK_NONE;

  state_set_installation_state(&new_state);
  firmware_updated = true;
}
