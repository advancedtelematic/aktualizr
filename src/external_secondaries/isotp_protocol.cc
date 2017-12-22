#include <stdint.h>

extern "C" {

uint32_t protocol_swap_sa_ta(uint32_t af) {
  uint32_t res = (af >> 5) & 0x1F;

  res |= (af & 0x1F) << 5;

  return res;
}
}
