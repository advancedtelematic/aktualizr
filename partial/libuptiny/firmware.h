#ifndef LIBUPTINY_FIRMWARE_H_
#define LIBUPTINY_FIRMWARE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

bool uptane_verify_firmware_init(void);
void uptane_verify_firmware_feed(const uint8_t* data, size_t len);
bool uptane_verify_firmware_finalize(void);
void uptane_firmware_confirm(void);

#ifdef __cplusplus
}
#endif

#endif  // LIBUPTINY_FIRMWARE_H_
