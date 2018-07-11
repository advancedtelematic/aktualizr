#include "utils.h"

static inline bool is_hex(uint8_t c) {
	return (c >= '0' && c <= '9') ||
	       (c >= 'A' && c <= 'F') ||
	       (c >= 'a' && c <= 'f');
}

static inline uint8_t from_hex(char sym) {
  if(sym >= '0' && sym <= '9') {
    return sym - '0';
  } else if (sym >= 'A' && sym <= 'F') {
    return sym - 'A' + 10;
  } else { /* inputs are validated by the caller */
    return sym - 'a' + 10;
  }
}

bool hex2bin(const char *hex_string, int hex_len, uint8_t *bin_data) {
  for(int i = 0; i < hex_len; ++i) {
    char sym = hex_string[i];
    if(!is_hex(sym)) {
      return false;
    }

    if(i & 1) {
      bin_data[i>>1] |= from_hex(sym);
    } else {
      bin_data[i>>1] = from_hex(sym) << 4;
    }
  }  
  return true;
}

int hex_bin_cmp(const char *hex_string, int hex_len, const uint8_t *bin_data) {
  uint8_t byte = 0;
  for(int i = 0; i < hex_len; ++i) {
    char sym = hex_string[i];
    if(!is_hex(sym)) {
      return -1; // Invalid string is "less" than anything
    }

    if(i & 1) {
      byte |= from_hex(sym);
      if(byte < bin_data[i>>1]) {
        return -1;
      } else if(byte > bin_data[i>>1]) {
        return 1;
      }
    } else {
      byte = from_hex(sym) << 4;
    }
  }  
  return 0;
}

bool dec2int(const char *dec_string, int dec_len, int32_t *out) {
  int32_t res = 0;
  bool neg = 0;
  int i = 0;
  if(dec_string[0] == '-') {
    neg = 1;
    ++i;
  }

  for(;i < dec_len; i++) {
    if (dec_string[i] < '0' || dec_string[i] > '9') {
      return false;
    }
    res *= 10;
    res += dec_string[i] - '0';
  }

  *out = ((neg) ? -res : res);
  return true;
}

/* Time format in UPTANE: YYYY-MM-DDThh:mm:ssZ. The function ignores delimiters. */
bool str2time(const char *time_string, int time_len, uptane_time_t* out) {
  int32_t num;

  if(time_len != 20) {
    return false;
  }

  if(!dec2int(time_string, 4, &num)) {
    return false;
  }
  out->year = num;

  if(!dec2int(time_string+5, 2, &num)) {
    return false;
  }
  out->month = num;

  if(!dec2int(time_string+8, 2, &num)) {
    return false;
  }
  out->day = num;

  if(!dec2int(time_string+11, 2, &num)) {
    return false;
  }
  out->hour = num;

  if(!dec2int(time_string+14, 2, &num)) {
    return false;
  }
  out->minute = num;

  if(!dec2int(time_string+17, 2, &num)) {
    return false;
  }
  out->second = num;
  return true;
}
