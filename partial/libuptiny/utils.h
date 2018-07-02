#ifndef LIBUPTINY_UTILS_H_
#define LIBUPTINY_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

int hex_bin_cmp(const char *hex_string, int hex_len, const uint8_t *bin_data);
bool hex2bin(const char *hex_string, int hex_len, uint8_t *bin_data);
#endif // LIBUPTINY_UTILS_H_
