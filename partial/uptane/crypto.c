#include "crypto.h"
#include "edsign.h"
#include <stdlib.h>
#include <string.h>
struct crypto_verify_ctx {
	size_t bytes_fed;
	uint8_t block[SHA512_BLOCK_SIZE];
	struct sha512_state sha_state;
	const uint8_t* signature;
	const uint8_t* pub;
};

static const char* supported_keytypes[] = {"ed25519"};

#ifdef CONFIG_UPTANE_NOMALLOC
static crypto_verify_ctx_t crypto_verify_ctxs[CONFIG_UPTANE_VERIFY_NUM_CONTEXTS];
static bool crypto_verify_ctx_busy[CONFIG_UPTANE_VERIFY_NUM_CONTEXTS] = {0, }; //could be reduce to a bitmask when new/free performance is an issue

static crypto_key_and_signature_t crypto_sigs[CONFIG_UPTANE_CRYPTO_NUM_SIGS];
static bool crypto_sig_busy[CONFIG_UPTANE_CRYPTO_NUM_SIGS] = {0, };
#endif

crypto_verify_ctx_t* crypto_verify_ctx_new() {
#ifdef CONFIG_UPTANE_NOMALLOC
	int i;

	for(i = 0; i < CONFIG_UPTANE_VERIFY_NUM_CONTEXTS; i++)
		if(!crypto_verify_ctx_busy[i])
			return &crypto_verify_ctxs[i];
	return NULL;
#else
	return malloc(sizeof(crypto_verify_ctx_t));
#endif
}

void crypto_verify_ctx_free(crypto_verify_ctx_t* ctx) {
	int i;
#ifdef CONFIG_UPTANE_NOMALLOC
	uintptr_t j = ((uintptr_t) ctx - (uintptr_t) crypto_verify_ctxs)/sizeof(crypto_verify_ctx_t); 
	crypto_verify_ctx_busy[j] = 0;
#else
	free(ctx);
#endif
}

crypto_key_and_signature_t* crypto_sig_new() {
#ifdef CONFIG_UPTANE_NOMALLOC
	int i;

	for(i = 0; i < CONFIG_UPTANE_CRYPTO_NUM_SIGS; i++)
		if(!crypto_sig_busy[i])
			return &crypto_sigs[i];
	return NULL;
#else
	return malloc(sizeof(crypto_key_and_signature_t));
#endif
}

void crypto_sig_free(crypto_key_and_signature_t* ctx) {
	int i;
#ifdef CONFIG_UPTANE_NOMALLOC
	uintptr_t j = ((uintptr_t) ctx - (uintptr_t) crypto_sigs)/sizeof(crypto_key_and_signature_t); 
	crypto_sig_busy[j] = 0;
#else
	free(ctx);
#endif
}


void crypto_verify_init(crypto_verify_ctx_t* ctx, crypto_key_and_signature_t* sig) {
	ctx->bytes_fed = 0;
	ctx->signature = sig->sig;
	ctx->pub = sig->key->keyval;
}

void crypto_verify_feed(crypto_verify_ctx_t* ctx, const uint8_t* data, int len) {
	int i;

	for(i = 0; i < len; i++) {
		if(ctx->bytes_fed < SHA512_BLOCK_SIZE - 64) {
			ctx->block[ctx->bytes_fed++] = data[i];

			/* First block is ready */
			if(ctx->bytes_fed == SHA512_BLOCK_SIZE - 64)
				edsign_verify_init(&ctx->sha_state, ctx->signature,
						   ctx->pub, ctx->block,
						   SHA512_BLOCK_SIZE - 64);
		} else {
			/* Trust compiler to use masking instead of actual division */
			int ind = (ctx->bytes_fed - (SHA512_BLOCK_SIZE - 64))
				   % SHA512_BLOCK_SIZE;
			ctx->block[ind] = data[i];

			if(ind == SHA512_BLOCK_SIZE - 1)
				edsign_verify_block(&ctx->sha_state, ctx->block);
			++ctx->bytes_fed;
		}
	}
}

bool crypto_verify_result(crypto_verify_ctx_t* ctx) {
	if(ctx->bytes_fed < SHA512_BLOCK_SIZE)
		return edsign_verify_init(&ctx->sha_state, ctx->signature,
					   ctx->pub, ctx->block, ctx->bytes_fed);
	else
		return edsign_verify_final(&ctx->sha_state, ctx->signature,
					   ctx->pub, ctx->block, ctx->bytes_fed);
}

bool crypto_keytype_supported(const char* keytype) {
	int i;
	for(i = 0; i < sizeof(supported_keytypes)/sizeof(supported_keytypes[0]); i++)
		if(!strcmp(keytype, supported_keytypes[i]))
			return true;

	return false;
}
