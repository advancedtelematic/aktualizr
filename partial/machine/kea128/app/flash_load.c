#include "flash_load.h"
#include "flash.h"

#include <string.h>

/* Flash memory (128K) is mapped on 0x00000-0x1FFFF
 * Sectors are 512 bytes long.
 * Boundary for start/end addresses are left to the caller to
 * differentiate between boundary errors and flash failures */

static uint8_t buf[FLASH_SECTOR_SIZE];

static uint32_t flash_load_addr;
static uint32_t flash_load_curaddr;
static uint32_t flash_load_size;

/* flash_start_address is 0 on real device, but can be overriden for testing*/
extern uint32_t flash_start_address;


int flash_load_erase(uint32_t start, uint32_t size) {
	uint32_t end = start+size-1; /* The last address to be cleared */
	uint32_t head = start & 0x1ff; /* modulo 512 */
	uint32_t tail = (end+1) & 0x1ff; /* modulo 512 */
	uint32_t startsec = start >> 9;
	uint32_t endsec = end >> 9;
	uint32_t i;

	if(head) {
		memcpy(buf, (void*)((startsec << 9)+flash_start_address), head);
		memset(buf+head, 0xFF, 512-head);
	}

	if(startsec != endsec) {
		/* Erase all sectors but the last one */
		for(i = startsec; i < endsec; i++)
			if(!flash_erase_sector(i << 9))
				return 0;

		if(head)
			if(!flash_write_sector(startsec << 9, buf))
				return 0;

		if(tail) {
			memset(buf, 0xFF, tail);
			memcpy(buf+tail, (void*)((endsec << 9) + tail + flash_start_address), 512-tail);
		}
	} else {
		/* Tail is always greater than head in one-sector case, because end > start is a prerequisite*/
		if(tail)
			memcpy(buf+tail, (void*)((endsec << 9) + tail + flash_start_address), 512-tail);
	}

	/* Erase the last sector, keeping unaffected bytes */
	if(!flash_erase_sector(endsec << 9))
		return 0;
	if(!flash_write_sector(endsec << 9, buf))
		return 0;
	return 1;
}

int flash_load_prepare(uint32_t addr, uint32_t size) {
	uint32_t flash_load_head;

	flash_load_addr = flash_load_curaddr = addr;
	flash_load_size = size;
	
	flash_load_head = flash_load_addr & 0x1FF;
	memcpy(buf, (void*)((flash_load_addr & ~0x1FF) + flash_start_address), flash_load_head);

	return 1;
}

int flash_load_continue(const uint8_t* data, uint32_t len) {
	uint32_t i_buf = flash_load_curaddr & 0x1FF;
	int i;

	for(i = 0; i < len; i++) {
		buf[i_buf] = data[i];
		if(i_buf == 0x1FF) {
			if(!flash_write_sector(flash_load_curaddr & ~0x1FF, buf))
				return 0;
			i_buf = 0;
		} else {
			i_buf++;
		}
		flash_load_curaddr++;
	}
	return 1;
}

int flash_load_finalize() {
	uint32_t tail = flash_load_curaddr & 0x1FF;
	memcpy(buf+tail, (uint8_t*) (flash_load_curaddr + flash_start_address), 512-tail);
	return flash_write_sector(flash_load_curaddr & ~0x1FF, buf);
}

