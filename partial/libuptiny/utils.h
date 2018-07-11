#ifndef LIBUPTINY_UTILS_H_
#define LIBUPTINY_UTILS_H_

#include <stdint.h>
#include <stdbool.h>
#include "uptane_time.h"

int hex_bin_cmp(const char *hex_string, int hex_len, const uint8_t *bin_data);
bool hex2bin(const char *hex_string, int hex_len, uint8_t *bin_data);
bool dec2int(const char *dec_string, int dec_len, int32_t *out);
bool str2time(const char *time_string, int time_len, uptane_time_t* out);
#endif // LIBUPTINY_UTILS_H_
