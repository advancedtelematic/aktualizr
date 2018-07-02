#ifndef LIBUPTINY_BASE64_H
#define LIBUPTINY_BASE64_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// size of a buffer to hold a zero-terminated base64 encoding of an msg_size large data
#define BASE64_ENCODED_BUF_SIZE(msg_size) (((msg_size) <= 0) ? 0 : 4*(1+((msg_size)-1)/3) + 1)

// size of a buffer to hold data encoded as a base64 string of length base64_len
#define BASE64_DECODED_BUF_SIZE(base64_len) (((base64_len)/4)*3)
/**
 * Encode binary data as base64 string
 * @param msg Data to encode
 * @param msg_len Length of data to encode
 * @param out Pointer to the buffer for encoded data. Data is zero-terminated, the buffer should be at least BASE64_ENCODED_BUF_SIZE(msg_len) bytes large
 */
void base64_encode(const uint8_t *msg, size_t msg_len, char *out);

/**
 * Decode zero-terminated base64 string
 * @param base64 String to encode
 * @param out Pointer to the buffer for decoded data. The buffer should be at least BASE64_DECODED_BUF_SIZE(strlen(base64)) bytes large
 * @return Size of decoded data or a negative value on failure
 */
ssize_t base64_decode(const char *base64, size_t base64_len, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif // LIBUPTINY_BASE64_H
