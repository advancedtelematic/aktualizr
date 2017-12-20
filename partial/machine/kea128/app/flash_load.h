#ifndef ATS_BOOT_FLASH_LOADER_H
#define ATS_BOOT_FLASH_LOADER_H

#include <stdint.h>

#define PROGRAM_FLASH_BEGIN 0x08000
#define PROGRAM_FLASH_END 0x20000

int flash_load_erase(uint32_t start, uint32_t size);
int flash_load_prepare(uint32_t addr, uint32_t size);
int flash_load_continue(const uint8_t* data, uint32_t len);
int flash_load_finalize(void);

#endif /* ATS_BOOT_FLASH_LOADER_H */
