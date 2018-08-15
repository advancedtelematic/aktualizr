#include "utils.h"

static inline bool is_hex(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }

static inline uint8_t from_hex(char sym) {
  if (sym >= '0' && sym <= '9') {
    return (uint8_t)(sym - '0');
  } else if (sym >= 'A' && sym <= 'F') {
    return (uint8_t)(sym - 'A' + 10);
  } else { /* inputs are validated by the caller */
    return (uint8_t)(sym - 'a' + 10);
  }
}

bool hex2bin(const char *hex_string, int hex_len, uint8_t *bin_data) {
  for (int i = 0; i < hex_len; ++i) {
    char sym = hex_string[i];
    if (!is_hex(sym)) {
      return false;
    }

    if (i & 1) {
      bin_data[i >> 1] |= from_hex(sym);
    } else {
      bin_data[i >> 1] = (uint8_t)(from_hex(sym) << 4);
    }
  }
  return true;
}

static inline char to_hex(uint8_t nibble) {
  if (nibble <= 9) {
    return (char)('0' + nibble);
  } else {
    return (char)('a' + nibble - 10);
  }
}

void bin2hex(const uint8_t *bin_data, int bin_len, char *hex_string) {
  for (int i = 0; i < bin_len; ++i) {
    hex_string[i << 1] = to_hex(bin_data[i] >> 4);
    hex_string[(i << 1) + 1] = to_hex(bin_data[i] & 0x0F);
  }
  hex_string[bin_len << 1] = 0;
}

int hex_bin_cmp(const char *hex_string, int hex_len, const uint8_t *bin_data) {
  uint8_t byte = 0;
  for (int i = 0; i < hex_len; ++i) {
    char sym = hex_string[i];
    if (!is_hex(sym)) {
      return -1;  // Invalid string is "less" than anything
    }

    if (i & 1) {
      byte |= from_hex(sym);
      if (byte < bin_data[i >> 1]) {
        return -1;
      } else if (byte > bin_data[i >> 1]) {
        return 1;
      }
    } else {
      byte = (uint8_t)(from_hex(sym) << 4);
    }
  }
  return 0;
}

bool dec2int(const char *dec_string, int dec_len, int32_t *out) {
  int32_t res = 0;
  bool neg = 0;
  int i = 0;
  if (dec_string[0] == '-') {
    neg = 1;
    ++i;
  }

  for (; i < dec_len; i++) {
    if (dec_string[i] < '0' || dec_string[i] > '9') {
      return false;
    }
    res *= 10;
    res += dec_string[i] - '0';
  }

  *out = ((neg) ? -res : res);
  return true;
}

void int2dec(int32_t num, char *dec_string) {
  int first_digit = 0;
  if (num < 0) {
    dec_string[0] = '-';
    num = -num;
    first_digit = 1;
  } else if (num == 0) {
    dec_string[0] = '0';
    dec_string[1] = 0;
    return;
  }

  int idx = first_digit;
  while (num) {
    dec_string[idx++] = (char)('0' + (num % 10));
    num /= 10;
  }
  dec_string[idx] = 0;
  int last_digit = idx - 1;

  for (int i = 0; i < (last_digit + 1) / 2; ++i) {
    char c = dec_string[first_digit + i];
    dec_string[first_digit + i] = dec_string[last_digit - i];
    dec_string[last_digit - i] = c;
  }
}

/* Time format in UPTANE: YYYY-MM-DDThh:mm:ssZ. The function ignores delimiters. */
bool str2time(const char *time_string, int time_len, uptane_time_t *out) {
  int32_t num;

  if (time_len != 20) {
    return false;
  }

  if (!dec2int(time_string, 4, &num) || num < 0 || num >= (1 << 16)) {
    return false;
  }
  out->year = (uint16_t)num;

  if (!dec2int(time_string + 5, 2, &num) || num < 0 || num >= (1 << 8)) {
    return false;
  }
  out->month = (uint8_t)num;

  if (!dec2int(time_string + 8, 2, &num) || num < 0 || num >= (1 << 8)) {
    return false;
  }
  out->day = (uint8_t)num;

  if (!dec2int(time_string + 11, 2, &num) || num < 0 || num >= (1 << 8)) {
    return false;
  }
  out->hour = (uint8_t)num;

  if (!dec2int(time_string + 14, 2, &num) || num < 0 || num >= (1 << 8)) {
    return false;
  }
  out->minute = (uint8_t)num;

  if (!dec2int(time_string + 17, 2, &num) || num < 0 || num >= (1 << 8)) {
    return false;
  }
  out->second = (uint8_t)num;
  return true;
}
