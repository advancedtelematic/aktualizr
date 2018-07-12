#include "state_api.h"
#include <memory>

crypto_key_t key = {
  .key_type = ED25519,
  .keyid = {0xa7, 0x0a, 0x72, 0x56, 0x14, 0x09, 0xb9, 0xe0, 0xbc, 0x67, 0xb7, 0x62, 0x58, 0x65, 0xfe, 0xd8, 0x01, 0xa5, 0x77, 0x71, 0x10, 0x25, 0x14, 0xb6, 0xde, 0x5f, 0x3b, 0x85, 0xf1, 0xbf, 0x27, 0xc2},
  .keyval = {0xC1, 0x81, 0x43, 0xA7, 0x3B, 0xD7, 0xEC, 0x00, 0xC0, 0xAC, 0xC1, 0x94, 0xCA, 0xD9, 0x73, 0x11, 0x8B, 0xDE, 0x92, 0x0B, 0xF4, 0xC0, 0x62, 0x9F, 0x7F, 0x95, 0x20, 0x9D, 0x42, 0x28, 0x3C, 0xB6},
};

crypto_key_t *keys[] = {&key};

std::unique_ptr<uptane_root_t> stored_root;
extern "C" {
  uptane_root_t* state_get_root(void) {
    if( !stored_root ) {
      stored_root = std::unique_ptr<uptane_root_t>(new uptane_root_t);
      stored_root->version = 0;
      stored_root->expires = {2038, 1, 1, 0, 0, 0};
      stored_root->root_threshold = 1;
      stored_root->root_keys_num = 1; 
      stored_root->root_keys[0] = &key;
      stored_root->targets_threshold = 1;
      stored_root->targets_keys_num = 1; 
      stored_root->targets_keys[0] = &key;
    }
    return stored_root.get();
  }

  void state_set_root(const uptane_root_t* root) {
    stored_root->version = root->version;
    stored_root->expires = root->expires;
    stored_root->root_threshold = root->root_threshold;
    stored_root->root_keys_num = root->root_keys_num;
    for(int i = 0; i < root->root_keys_num; ++i) { 
      stored_root->root_keys[i] = root->root_keys[i];
    }
    stored_root->targets_threshold = 1;
    stored_root->targets_keys_num = 1; 
    for(int i = 0; i < root->targets_keys_num; ++i) { 
      stored_root->targets_keys[i] = root->targets_keys[i];
    }
  }

  const char* state_get_ecuid(void) {
    return "uptane_secondary_1";
  }

  const char* state_get_hwid(void) {
    return "test_uptane_secondary";
  }
}
