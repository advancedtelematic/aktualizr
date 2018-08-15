#ifndef LIBUPTINY_UTILS_H_
#define LIBUPTINY_UTILS_H_

#include <stdbool.h>
#include <stdint.h>
#include "uptane_time.h"

int hex_bin_cmp(const char *hex_string, int hex_len, const uint8_t *bin_data);
bool hex2bin(const char *hex_string, int hex_len, uint8_t *bin_data);
void bin2hex(const uint8_t *bin_data, int bin_len, char *hex_string);
bool dec2int(const char *dec_string, int dec_len, int32_t *out);
void int2dec(int32_t num, char *dec_string);
bool str2time(const char *time_string, int time_len, uptane_time_t *out);
#endif  // LIBUPTINY_UTILS_H_
