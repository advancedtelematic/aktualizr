#include "state_api.h"
#include <memory>

crypto_key_t key = {
  .key_type = ED25519,
  .keyid = {0x98, 0x2d, 0xaa, 0xe1, 0xf8, 0xbc, 0x4c, 0x81, 0xe2, 0x59, 0x11, 0x2c, 0x0b, 0xaa, 0xf3, 0xca, 0x49, 0xa2, 0x0b, 0xac, 0x17, 0x20, 0x44, 0xec, 0x73, 0x58, 0x68, 0xec, 0x98, 0xc3, 0xf4, 0x06},
  .keyval = {0x5D, 0xF6, 0x19, 0x4E, 0x7A, 0x91, 0x0D, 0xFD, 0xF7, 0xDF, 0xF1, 0xC1, 0x29, 0x59, 0x5F, 0x5D, 0xEE, 0x0D, 0xF1, 0x82, 0x1D, 0x78, 0xFD, 0x28, 0xCA, 0x42, 0x7F, 0x18, 0xF8, 0x52, 0xFD, 0x02},
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
}
