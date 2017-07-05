/* Arithmetic in prime fields
 * Daniel Beer <dlbeer@gmail.com>, 10 Jan 2014
 *
 * This file is in the public domain.
 */

#ifndef FPRIME_H_
#define FPRIME_H_

#include <stdint.h>
#include <string.h>

/* Maximum size of a field element (or a prime). Field elements are
 * always manipulated and stored in normalized form, with 0 <= x < p.
 * You can use normalize() to convert a denormalized bitstring to normal
 * form.
 *
 * Operations are constant with respect to the value of field elements,
 * but not with respect to the modulus.
 *
 * The modulus is a number p, such that 2p-1 fits in FPRIME_SIZE bytes.
 */
#define FPRIME_SIZE		32

/* Useful constants */
extern const uint8_t fprime_zero[FPRIME_SIZE];
extern const uint8_t fprime_one[FPRIME_SIZE];

/* Load a large constant */
void fprime_from_bytes(uint8_t *x,
		       const uint8_t *in, size_t len,
		       const uint8_t *modulus);

/* Copy an element */
static inline void fprime_copy(uint8_t *x, const uint8_t *a)
{
	memcpy(x, a, FPRIME_SIZE);
}

void fprime_select(uint8_t *dst,
		   const uint8_t *zero, const uint8_t *one,
		   uint8_t condition);

/* Add one value to another. The two pointers must be distinct. */
void fprime_add(uint8_t *r, const uint8_t *a, const uint8_t *modulus);

/* Multiply two values to get a third. r must be distinct from a and b */
void fprime_mul(uint8_t *r, const uint8_t *a, const uint8_t *b,
		const uint8_t *modulus);


#endif
