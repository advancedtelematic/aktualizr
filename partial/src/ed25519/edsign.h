/* Edwards curve signature system
 * Daniel Beer <dlbeer@gmail.com>, 22 Apr 2014
 *
 * This file is in the public domain.
 */

#ifndef EDSIGN_H_
#define EDSIGN_H_

#include <stdint.h>
#include "sha512.h"

/* This is the Ed25519 signature system, as described in:
 *
 *     Daniel J. Bernstein, Niels Duif, Tanja Lange, Peter Schwabe, Bo-Yin
 *     Yang. High-speed high-security signatures. Journal of Cryptographic
 *     Engineering 2 (2012), 77–89. Document ID:
 *     a1a62a2f76d23f65d622484ddd09caf8. URL:
 *     http://cr.yp.to/papers.html#ed25519. Date: 2011.09.26. 
 *
 * The format and calculation of signatures is compatible with the
 * Ed25519 implementation in SUPERCOP. Note, however, that our secret
 * keys are half the size: we don't store a copy of the public key in
 * the secret key (we generate it on demand).
 */

/* Any string of 32 random bytes is a valid secret key. There is no
 * clamping of bits, because we don't use the key directly as an
 * exponent (the exponent is derived from part of a key expansion).
 */
#define EDSIGN_SECRET_KEY_SIZE		32

/* Given a secret key, produce the public key (a packed Edwards-curve
 * point).
 */
#define EDSIGN_PUBLIC_KEY_SIZE		32

void edsign_sec_to_pub(uint8_t *pub, const uint8_t *secret);

/* Produce a signature for a message. */
#define EDSIGN_SIGNATURE_SIZE		64

#define EDSIGN_BLOCK_SIZE		SHA512_BLOCK_SIZE
typedef struct sha512_state edverify_state_t;

void edsign_sign(uint8_t *signature, const uint8_t *pub,
		 const uint8_t *secret,
		 const uint8_t *message, size_t len);

/* start verification. Len should be not larger than SHA512_BLOCK_SIZE - 64 bytes of data
 *   If len is less that that, then it's considered to be the whole message, otherwise
 *   edsign_verify_block and edsign_verify_finalize should be called to verify the rest
 *   of the message. When used to verify the whole message, non-zero return indicates
 *   success, otherwise it should be ignored.
 */
uint8_t edsign_verify_init(struct sha512_state* s, const uint8_t *signature,
			const uint8_t *pub, const uint8_t *message, size_t len);

// add block of size SHA512_BLOCK_SIZE to stream
void edsign_verify_block(struct sha512_state* s, const uint8_t *message);

// add last block and return result. message points to the last block, not the whole message
//  Non-zero indicates success
uint8_t edsign_verify_final(struct sha512_state* s, const uint8_t *signature,
			       const uint8_t* pub, const uint8_t *message,
			       size_t total_len);

#endif
