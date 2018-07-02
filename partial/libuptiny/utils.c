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

