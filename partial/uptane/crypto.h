#ifndef AKTUALIZR_PARTIAL_CRYPTO_H_
#define AKTUALIZR_PARTIAL_CRYPTO_H_

#include <stdint.h>
#include <stdbool.h>
#include "sha512.h"

#define CRYPTO_KEYID_LEN 32 // SHA256 hash
#define CRYPTO_KEYVAL_LEN 32 // public key length, 32 for ed25519
#define CRYPTO_SIGNATURE_LEN 64 // signature length, 64 for ed25519

typedef enum {ED25519} crypto_algorithm_t;
typedef struct {
	crypto_algorithm_t key_type;
	uint8_t keyid[CRYPTO_KEYID_LEN];
	uint8_t keyval[CRYPTO_KEYVAL_LEN];
} crypto_key_t;

typedef struct {
	const crypto_key_t* key;
	uint8_t sig[CRYPTO_SIGNATURE_LEN];
} crypto_key_and_signature_t;

typedef struct crypto_verify_ctx crypto_verify_ctx_t;

crypto_verify_ctx_t* crypto_verify_ctx_new(void);
void crypto_verify_ctx_free(crypto_verify_ctx_t* ctx);

crypto_key_and_signature_t* crypto_sig_new(void);
void crypto_sig_free(crypto_key_and_signature_t* sig);

void crypto_verify_init(crypto_verify_ctx_t* ctx, crypto_key_and_signature_t* sig);
void crypto_verify_feed(crypto_verify_ctx_t* ctx, const uint8_t* data, int len);
bool crypto_verify_result(crypto_verify_ctx_t* ctx);

bool crypto_keytype_supported(const char* keytype);

#endif //AKTUALIZR_PARTIAL_CRYPTO_H
