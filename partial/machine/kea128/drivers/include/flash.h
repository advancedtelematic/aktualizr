#ifndef ATS_DRIVERS_FLASH_H
#define ATS_DRIVERS_FLASH_H

#include <stdint.h>

#define FLASH_SECTOR_SIZE 512
#define FLASH_SIZE 0x20000

void flash_init(void);
int flash_erase_sector(uint32_t addr);
int flash_write_sector(uint32_t addr, const uint8_t* data);

#endif /* ATS_DRIVERS_FLASH_H */

