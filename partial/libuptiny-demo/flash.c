#include "flash.h"
#include "vendor/SKEAZ1284.h"

static void command(int len, const uint8_t* cmd) {
  volatile int i;

  if (len <= 0 || len > 12) return;

  while (!(FTMRE->FSTAT & (1 << 7)))
    ;  // wait for CCIF

  if (FTMRE->FSTAT & (1 << 4))  // FPVIOL
    FTMRE->FSTAT = (1 << 4);    // Clear
  if (FTMRE->FSTAT & (1 << 5))  // ACCERR
    FTMRE->FSTAT = (1 << 5);    // Clear

  for (i = 0; i < len; i++) {
    if (!(i & 1)) {  // even byte
      FTMRE->FCCOBIX = i >> 1;
      FTMRE->FCCOBHI = cmd[i];
    } else {
      FTMRE->FCCOBLO = cmd[i];
    }
  }

  // enable stalls
  MCM->PLACR |= (1 << 16);
  FTMRE->FSTAT = 0x80;
  while (!(FTMRE->FSTAT & 0x80))
    ;  // wait for CCIF
  // disable stalls (p. 202 of reference manual)
  MCM->PLACR &= (1 << 16);
}

int flash_erase_sector(uint32_t addr) {
  uint8_t cmd[4];

  // address is not aligned
  if (addr % FLASH_SECTOR_SIZE) return 0;

  // out of range
  if (addr >= FLASH_SIZE) return 0;

  cmd[0] = 0x0A;
  cmd[1] = (addr >> 16) & 0xFF;
  cmd[2] = (addr >> 8) & 0xFF;
  cmd[3] = addr & 0xFF;

  command(4, cmd);

  return !(FTMRE->FSTAT & 0x33);  // ACCERR, FPVIOL and MSGSTAT are cleared
}

// programs exactly 8 bytes of flash. Address should be aligned by 8.
static int program_flash(uint32_t addr, const uint8_t* data) {
  uint8_t cmd[12];
  int i;

  cmd[0] = 0x06;
  cmd[1] = (addr >> 16) & 0xFF;
  cmd[2] = (addr >> 8) & 0xFF;
  cmd[3] = addr & 0xFF;
  for (i = 0; i < 8; i++) {
    // swap LO/HI to preserve byte order
    if (i & 1)
      cmd[4 + i - 1] = data[i];
    else
      cmd[4 + i + 1] = data[i];
  }

  command(12, cmd);

  return !(FTMRE->FSTAT & 0x33);  // ACCERR, FPVIOL and MSGSTAT are cleared
}

void flash_init(void) {
  uint8_t fclk = (SystemCoreClock / 2) / 1000000 - 1;

  if ((FTMRE->FCLKDIV & 0x3F) != fclk) FTMRE->FCLKDIV = fclk;
}

int flash_write_sector(uint32_t addr, const uint8_t* data) {
  int i;
  // address is not aligned
  if (addr % FLASH_SECTOR_SIZE) return 0;

  // out of range
  if (addr >= FLASH_SIZE) return 0;

  if (!flash_erase_sector(addr)) return 0;

  for (i = 0; i < FLASH_SECTOR_SIZE; i += 8)
    if (!program_flash(addr + i, data + i)) return 0;

  return 1;
}
