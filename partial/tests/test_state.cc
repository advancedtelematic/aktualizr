#include "state_api.h"
#include "crypto/crypto.h"
#include <iostream>
#include "utilities/utils.h"
#include <memory>

class TestKeyReader {
 public:
  TestKeyReader(boost::filesystem::path path) : public_key{Utils::readFile(path), KeyType::kED25519} {;}
  crypto_key_t getKey() const {
    crypto_key_t res;
    res.key_type = CRYPTO_ALG_ED25519;
    std::string keyid_bin = boost::algorithm::unhex(public_key.KeyId());
    std::string keyval_bin = boost::algorithm::unhex(public_key.Value());
    std::copy(keyid_bin.cbegin(), keyid_bin.cend(), res.keyid);
    std::copy(keyval_bin.cbegin(), keyval_bin.cend(), res.keyval);
    return res;
  }

 private:
  PublicKey public_key;
};
crypto_key_t key = TestKeyReader(boost::filesystem::path("partial/tests/repo/keys/director/public.key")).getKey();

crypto_key_t *keys[] = {&key};

std::unique_ptr<uptane_root_t> stored_root;
uptane_root_t out_stored_root;

std::unique_ptr<uptane_targets_t> stored_targets;
uptane_targets_t out_stored_targets;

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
    memcpy(&out_stored_root, stored_root.get(), sizeof(uptane_root_t));
    return &out_stored_root;
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

  uptane_targets_t* state_get_targets(void) {
    if( !stored_targets ) {
      stored_targets = std::unique_ptr<uptane_targets_t>(new uptane_targets_t);
      stored_targets->version = -1;
      stored_targets->expires = {2038, 1, 1, 0, 0, 0};
      stored_targets->name[0] = '\0';
      stored_targets->hashes_num = 0;
      stored_targets->length = 0;
    }
    memcpy(&out_stored_targets, stored_targets.get(), sizeof(uptane_targets_t));
    return &out_stored_targets;
  }

  void state_set_targets(const uptane_targets_t* targets) {
    stored_targets->version = targets->version;
    stored_targets->expires = targets->expires;
    strncpy(stored_targets->name, targets->name, TARGETS_MAX_NAME_LENGTH);
    stored_targets->hashes_num = targets->hashes_num;
    for (int i = 0; i < targets->hashes_num; ++i) {
      stored_targets->hashes[i].alg = targets->hashes[i].alg;
      memcpy(stored_targets->hashes[i].hash, targets->hashes[i].hash, CRYPTO_MAX_HASH_LEN);
    }

    stored_targets->length = targets->length;
  }


  const char* state_get_ecuid(void) {
    return "uptane_secondary_1";
  }

  const char* state_get_hwid(void) {
    return "test_uptane_secondary";
  }
}
