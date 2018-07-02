#include "base64.h"

/* Bitfields are not used to be as cross-platform as possible */

static inline uint8_t encode_6bit(uint8_t sextet) {
  if(sextet <= 25) {
    return 'A' + sextet;
  } else if(sextet <= 51) {
    return 'a' + sextet - 26;
  } else if(sextet <= 61) {
    return '0' + sextet - 52;
  } else if(sextet == 62) {
    return '+';
  } else { // boundaries are maintained by the caller
    return '/';
  }
}

static inline void encode_triple(const uint8_t *msg, int padding, char *out) {
  out[0] = encode_6bit(msg[0] >> 2);
  out[1] = encode_6bit(0x3F & ((padding < 2) ? (msg[0] << 4) | (msg[1] >> 4) : (msg[0] << 4)));
  switch(padding) {
    case 2:
      out[2] = '=';
      out[3] = '=';
      break;
    case 1:
      out[2] =  encode_6bit(0x3F & (msg[1] << 2));
      out[3] = '=';
      break;
    default:
      out[2] =  encode_6bit(0x3F & ((msg[1] << 2) | (msg[2] >> 6)));
      out[3] =  encode_6bit(0x3F & msg[2]);
      break;
  }
}

void base64_encode(const uint8_t *msg, size_t msg_len, char *out) {
  int j = 0;
  for(int i = 0; i < msg_len; i += 3, j += 4) {
    int padding;
    switch(msg_len - i) {
      case 1:
        padding = 2;
        break;
      case 2:
        padding = 1;
        break;
      default:
        padding = 0;
        break;
    }
    
    encode_triple(msg+i, padding, out+j);
  }
  out[j] = 0;
}

#define B64_SYM_OOR 0xFF
#define B64_SYM_PAD 0xFE
#define B64_SYM_EOL 0xFD
#define B64_SYM_FAIL_MASK 0xC0

static inline uint8_t decode_sym(char symbol) {
  if(symbol >= 'A' && symbol <= 'Z') {
    return symbol - 'A';
  } else if(symbol >= 'a' && symbol <= 'z') {
    return 26 + symbol - 'a';
  } else if(symbol >= '0' && symbol <= '9') {
    return 52 + symbol - '0';
  } else if(symbol == '+') {
    return 62;
  } else if(symbol == '/') {
    return 63;
  } else if(symbol == '=') {
    return B64_SYM_PAD;
  } else if(symbol == 0) {
    return B64_SYM_EOL;
  } else {
    return B64_SYM_OOR;
  }
}

// function can write arbitrary data to out[i>return_value-1 && i < 3]
static inline int decode_quadruple(const char *base64, uint8_t *out) {
  uint8_t sym;
  sym = decode_sym(base64[0]);

  if(sym == B64_SYM_EOL) {
    return 0;
  } else if(sym & B64_SYM_FAIL_MASK) {
    return -1;
  }
  out[0] = sym << 2;

  sym = decode_sym(base64[1]);
  if(sym & B64_SYM_FAIL_MASK) {
    return -1;
  }
  out[0] |= sym >> 4;
  out[1] = sym << 4;

  sym = decode_sym(base64[2]);
  if(sym == B64_SYM_PAD) {
    return 1;
  }
  if(sym & B64_SYM_FAIL_MASK) {
    return -1;
  }
  out[1] |= sym >> 2;
  out[2] = sym << 6;

  sym = decode_sym(base64[3]);
  if(sym == B64_SYM_PAD) {
    return 2;
  }
  if(sym & B64_SYM_FAIL_MASK) {
    return -1;
  }
  out[2] |= sym;

  return 3;
}

ssize_t base64_decode(const char *base64, size_t base64_len, uint8_t *out) {
  ssize_t size = 0;
  for(int i = 0; i < base64_len; i+=4) {
    int res = decode_quadruple(base64+i, out+size);
    if(res < 0) {
      return -1;
    } else if(res < 3) {
      return size+res;
    } else {
      size += 3;
    }
  }
  return size;
}

