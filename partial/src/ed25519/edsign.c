/* Edwards curve signature system
 * Daniel Beer <dlbeer@gmail.com>, 22 Apr 2014
 *
 * This file is in the public domain.
 */

#include "ed25519.h"
#include "sha512.h"
#include "fprime.h"
#include "edsign.h"

#define EXPANDED_SIZE		64

static const uint8_t ed25519_order[FPRIME_SIZE] = {
	0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
	0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static void expand_key(uint8_t *expanded, const uint8_t *secret)
{
	struct sha512_state s;

	sha512_init(&s);
	sha512_final(&s, secret, EDSIGN_SECRET_KEY_SIZE);
	sha512_get(&s, expanded, 0, EXPANDED_SIZE);
	ed25519_prepare(expanded);
}

static uint8_t upp(struct ed25519_pt *p, const uint8_t *packed)
{
	uint8_t x[F25519_SIZE];
	uint8_t y[F25519_SIZE];
	uint8_t ok = ed25519_try_unpack(x, y, packed);

	ed25519_project(p, x, y);
	return ok;
}

static void pp(uint8_t *packed, const struct ed25519_pt *p)
{
	uint8_t x[F25519_SIZE];
	uint8_t y[F25519_SIZE];

	ed25519_unproject(x, y, p);
	ed25519_pack(packed, x, y);
}

static void sm_pack(uint8_t *r, const uint8_t *k)
{
	struct ed25519_pt p;

	ed25519_smult(&p, &ed25519_base, k);
	pp(r, &p);
}

void edsign_sec_to_pub(uint8_t *pub, const uint8_t *secret)
{
	uint8_t expanded[EXPANDED_SIZE];

	expand_key(expanded, secret);
	sm_pack(pub, expanded);
}

static void hash_message_prefix_init(struct sha512_state *s, uint8_t *block,
				     int prefix_size, const uint8_t *m,
				     size_t len)
{
	sha512_init(s);

	memcpy(block + prefix_size, m, len);
	if (len < SHA512_BLOCK_SIZE - prefix_size) {
		sha512_final(s, block, len + prefix_size);
	} else {
		sha512_block(s, block);
	}
}

static void hash_message_init(struct sha512_state *s, const uint8_t *r,
			      const uint8_t *a, const uint8_t *m, size_t len)
{
	uint8_t block[SHA512_BLOCK_SIZE];


	memcpy(block, r, 32);
	memcpy(block + 32, a, 32);
	hash_message_prefix_init(s, block, 64, m, len);
}

static void hash_message_block(struct sha512_state *s, const uint8_t *m)
{
	sha512_block(s, m);
}

static void hash_message_final(struct sha512_state *s, const uint8_t *m, size_t total_len) {
	sha512_final(s, m, total_len);
}

static void hash_message_finalize(struct sha512_state *s, uint8_t* z)
{
	uint8_t hash[SHA512_HASH_SIZE];

	sha512_get(s, hash, 0, SHA512_HASH_SIZE);
	fprime_from_bytes(z, hash, SHA512_HASH_SIZE, ed25519_order);
}

static void hash_with_prefix(uint8_t *out_fp, uint8_t *init_block, int prefix_size,
			     const uint8_t *message, size_t len) {
	struct sha512_state s;
	size_t i;

	if(len <= SHA512_BLOCK_SIZE - prefix_size) {
		hash_message_prefix_init(&s, init_block, prefix_size, message, len);
		hash_message_finalize(&s, out_fp);
	} else {
		hash_message_prefix_init(&s, init_block, prefix_size, message, SHA512_BLOCK_SIZE - prefix_size);
		for(i = SHA512_BLOCK_SIZE - prefix_size; i < len - SHA512_BLOCK_SIZE; i += SHA512_BLOCK_SIZE)
			hash_message_block(&s, message+i);
		hash_message_final(&s, message+i, len+prefix_size);
		hash_message_finalize(&s, out_fp);
	}
}

static void generate_k(uint8_t *k, const uint8_t *kgen_key,
		       const uint8_t *message, size_t len)
{
	uint8_t block[SHA512_BLOCK_SIZE];

	memcpy(block, kgen_key, 32);
	hash_with_prefix(k, block, 32, message, len);
}

void edsign_sign(uint8_t *signature, const uint8_t *pub,
		 const uint8_t *secret,
		 const uint8_t *message, size_t len)
{
	uint8_t expanded[EXPANDED_SIZE];
	uint8_t e[FPRIME_SIZE];
	uint8_t s[FPRIME_SIZE];
	uint8_t k[FPRIME_SIZE];
	uint8_t z[FPRIME_SIZE];

	uint8_t hash_block[SHA512_BLOCK_SIZE];

	expand_key(expanded, secret);

	/* Generate k and R = kB */
	generate_k(k, expanded + 32, message, len);
	sm_pack(signature, k);

	/* Compute z = H(R, A, M) */
	memcpy(hash_block, signature, 32);
	memcpy(hash_block+32, pub, 32);
	hash_with_prefix(z, hash_block, 64, message, len);

	/* Obtain e */
	fprime_from_bytes(e, expanded, 32, ed25519_order);

	/* Compute s = ze + k */
	fprime_mul(s, z, e, ed25519_order);
	fprime_add(s, k, ed25519_order);
	memcpy(signature + 32, s, 32);
}

static uint8_t edsign_verify_finalize(struct sha512_state* s, const uint8_t *signature,
				      const uint8_t *pub, const uint8_t *message)
{
	struct ed25519_pt p;
	struct ed25519_pt q;
	uint8_t lhs[F25519_SIZE];
	uint8_t rhs[F25519_SIZE];
	uint8_t z[FPRIME_SIZE];
	uint8_t ok = 1;

	hash_message_finalize(s, z);

	/* sB = (ze + k)B = ... */
	sm_pack(lhs, signature + 32);

	/* ... = zA + R */
	ok &= upp(&p, pub);
	ed25519_smult(&p, &p, z);
	ok &= upp(&q, signature);
	ed25519_add(&p, &p, &q);
	pp(rhs, &p);

	/* Equal? */
	return ok & f25519_eq(lhs, rhs);
}

uint8_t edsign_verify_init(struct sha512_state* s, const uint8_t *signature,
			   const uint8_t *pub, const uint8_t *message, size_t len)
{
	hash_message_init(s, signature, pub, message, len);
	// TODO: isolate
	if(len < SHA512_BLOCK_SIZE - 64)
		return edsign_verify_finalize(s, signature, pub, message);
	return 0;
}

void edsign_verify_block(struct sha512_state* s, const uint8_t *message)
{
	hash_message_block(s, message);
}

uint8_t edsign_verify_final(struct sha512_state* s, const uint8_t *signature,
		            const uint8_t *pub, const uint8_t *message, size_t total_len)
{
	hash_message_final(s, message, total_len+64); 
	return edsign_verify_finalize(s, signature, pub, message);
}

