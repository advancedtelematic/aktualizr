#include <string.h>
#include "libuptiny/state_api.h"

uptane_root_t stored_root;

uptane_targets_t stored_targets;

uptane_installation_state_t stored_installation_state;
bool installation_state_present = false;

const crypto_key_t device_public_key = {
    .key_type = CRYPTO_ALG_ED25519,
    .keyid = {0x13, 0xf6, 0x59, 0xbb, 0xe5, 0x9f, 0xdf, 0xfb, 0xed, 0xae, 0x16, 0xd9, 0x63, 0x69, 0xf4, 0x59,
              0x24, 0x8c, 0x21, 0x70, 0x59, 0x78, 0x3f, 0x04, 0x25, 0xf5, 0xf4, 0x31, 0x93, 0xf3, 0xde, 0xcf},
    .keyval = {0xF6, 0x59, 0xF7, 0x02, 0xCC, 0xD2, 0x1F, 0x4F, 0x6F, 0xA8, 0x7B, 0x88, 0x51, 0x1E, 0x2F, 0x9C,
               0xD8, 0x5F, 0x6C, 0xF6, 0x95, 0x11, 0x4D, 0x29, 0x98, 0x9B, 0x1F, 0x0F, 0xE8, 0x86, 0xB3, 0x55},
};

const uint8_t device_private_key[] = {0x36, 0x5D, 0xF7, 0x1A, 0xEA, 0x71, 0x61, 0xCC, 0x58, 0x57, 0xF4,
                                      0x03, 0xE1, 0xE6, 0x1E, 0xBB, 0x54, 0xFD, 0x89, 0x09, 0xF9, 0xA5,
                                      0xA1, 0x39, 0xDF, 0x86, 0xE5, 0xAF, 0xA9, 0x3F, 0x24, 0x4C};

crypto_key_t server_public_key = {
    .key_type = CRYPTO_ALG_ED25519,
    .keyid = {0x13, 0xf6, 0x59, 0xbb, 0xe5, 0x9f, 0xdf, 0xfb, 0xed, 0xae, 0x16, 0xd9, 0x63, 0x69, 0xf4, 0x59,
              0x24, 0x8c, 0x21, 0x70, 0x59, 0x78, 0x3f, 0x04, 0x25, 0xf5, 0xf4, 0x31, 0x93, 0xf3, 0xde, 0xcf},
    .keyval = {0xF6, 0x59, 0xF7, 0x02, 0xCC, 0xD2, 0x1F, 0x4F, 0x6F, 0xA8, 0x7B, 0x88, 0x51, 0x1E, 0x2F, 0x9C,
               0xD8, 0x5F, 0x6C, 0xF6, 0x95, 0x11, 0x4D, 0x29, 0x98, 0x9B, 0x1F, 0x0F, 0xE8, 0x86, 0xB3, 0x55},
};

uptane_root_t* state_get_root(void) { return &stored_root; }

void state_set_root(const uptane_root_t* root) {
  /* TODO: write to flash*/
  stored_root.version = root->version;

  stored_root.expires.year = root->expires.year;
  stored_root.expires.month = root->expires.month;
  stored_root.expires.day = root->expires.day;
  stored_root.expires.hour = root->expires.hour;
  stored_root.expires.minute = root->expires.minute;
  stored_root.expires.second = root->expires.second;

  stored_root.root_threshold = root->root_threshold;
  stored_root.root_keys_num = root->root_keys_num;
  for (int i = 0; i < stored_root.root_keys_num; ++i) {
    stored_root.root_keys[i] = root->root_keys[i];
  }

  stored_root.targets_threshold = root->targets_threshold;
  stored_root.targets_keys_num = root->targets_keys_num;
  for (int i = 0; i < stored_root.targets_keys_num; ++i) {
    stored_root.targets_keys[i] = root->targets_keys[i];
  }
}

uptane_targets_t* state_get_targets(void) { return &stored_targets; }

void state_set_targets(const uptane_targets_t* targets) { memcpy(&stored_targets, targets, sizeof(uptane_targets_t)); }

const char* state_get_ecuid(void) { return "libuptiny_demo_secondary"; }

const char* state_get_hwid(void) { return "libuptiny_demo"; }

void state_set_installation_state(const uptane_installation_state_t* state) {
  memcpy(&stored_installation_state, state, sizeof(uptane_installation_state_t));
  installation_state_present = true;
}

uptane_installation_state_t* state_get_installation_state(void) {
  if (!installation_state_present) {
    return NULL;
  } else {
    return &stored_installation_state;
  }
}

crypto_hash_algorithm_t state_get_supported_hash(void) { return CRYPTO_HASH_SHA512; }

void state_set_attack(uptane_attack_t attack) {
  if (!installation_state_present) {
    installation_state_present = true;
  }
  stored_installation_state.attack = attack;
}

void state_get_device_key(const crypto_key_t** pub, const uint8_t** priv) {
  *pub = &device_public_key;
  *priv = device_private_key;
}

void state_init(void) {
  // TODO: read from flash && provisioning
  stored_root.version = 1;

  stored_root.expires.year = 3018;
  stored_root.expires.month = 1;
  stored_root.expires.day = 1;
  stored_root.expires.hour = 1;
  stored_root.expires.minute = 1;
  stored_root.expires.second = 1;

  stored_root.root_threshold = 1;
  stored_root.root_keys_num = 1;
  stored_root.root_keys[0] = &server_public_key;

  stored_root.targets_threshold = 1;
  stored_root.targets_keys_num = 1;
  stored_root.targets_keys[0] = &server_public_key;
}
