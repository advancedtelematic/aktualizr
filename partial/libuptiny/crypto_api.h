#ifndef LIBUPTINY_CRYPTO_API_H_
#define LIBUPTINY_CRYPTO_API_H_

#include <stdbool.h>
#include <stdint.h>
#include "sha512.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CRYPTO_KEYID_LEN 32         /* SHA256 hash */
#define CRYPTO_KEYVAL_LEN 32        /* public key length, 32 for ed25519 */
#define CRYPTO_MAX_SIGNATURE_LEN 64 /* enough to hold ed25519 signature */
#define CRYPTO_MAX_HASH_LEN 64      /* enough to hold sha512 hash */

typedef enum { CRYPTO_HASH_UNKNOWN = -1, CRYPTO_HASH_SHA512 = 0 } crypto_hash_algorithm_t;
typedef struct {
  crypto_hash_algorithm_t alg;
  uint8_t hash[CRYPTO_MAX_HASH_LEN];
} crypto_hash_t;

typedef enum { CRYPTO_ALG_UNKNOWN = -1, CRYPTO_ALG_ED25519 = 0 } crypto_algorithm_t;
typedef struct {
  crypto_algorithm_t key_type;
  uint8_t keyid[CRYPTO_KEYID_LEN];
  uint8_t keyval[CRYPTO_KEYVAL_LEN];
} crypto_key_t;

typedef struct {
  const crypto_key_t* key;
  uint8_t sig[CRYPTO_MAX_SIGNATURE_LEN];
} crypto_key_and_signature_t;

typedef struct {
  size_t bytes_fed;
  uint8_t block[SHA512_BLOCK_SIZE];
  struct sha512_state sha_state;
  const uint8_t* signature;
  const uint8_t* pub;
} crypto_verify_ctx_t;

void crypto_verify_init(crypto_verify_ctx_t* ctx, crypto_key_and_signature_t* sig);
void crypto_verify_feed(crypto_verify_ctx_t* ctx, const uint8_t* data, size_t len);
bool crypto_verify_result(crypto_verify_ctx_t* ctx);

crypto_algorithm_t crypto_str_to_keytype(const char* keytype, size_t len);
int crypto_get_keylen(crypto_algorithm_t alg);

crypto_hash_algorithm_t crypto_str_to_hashtype(const char* hashtype, size_t len);
int crypto_get_hashlen(crypto_hash_algorithm_t alg);
#ifdef __cplusplus
}
#endif

#endif  // LIBUPTINY_CRYPTO_API_H_
