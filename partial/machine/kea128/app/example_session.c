#include "isotp/protocol.h"

/* Example session level protocol
 * Addresses are 5 bit wide, normal addressing (CAN 2.0A) is used
 * TA is located in bits 0-4, SA - in bits 5-9, bit 10 is not used
 * */
uint32_t protocol_swap_sa_ta(uint32_t af) {
	uint32_t res = (af >> 5) & 0x1F;

	res |= (af & 0x1F) << 5;

	return res;
}
