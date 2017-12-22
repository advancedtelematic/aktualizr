#include "flash.h"
#include "flash_load.h"
#include <string.h>
#include <stdio.h>

uint8_t flash_mem[FLASH_SIZE];
uint8_t flash_mem_golden[FLASH_SIZE];

uint32_t flash_start_address;

int flash_write_sector(uint32_t addr, const uint8_t* data) {
	int i;

	// address is not aligned
        if(addr % FLASH_SECTOR_SIZE)
                return 0;

        //out of range
        if(addr >= FLASH_SIZE)
                return 0;

	memcpy(flash_mem+addr, data, FLASH_SECTOR_SIZE);
}

int flash_erase_sector(uint32_t addr) {
	int i;

	// address is not aligned
        if(addr % FLASH_SECTOR_SIZE)
                return 0;

        //out of range
        if(addr >= FLASH_SIZE)
                return 0;

	memset(flash_mem+addr, 0xff, FLASH_SECTOR_SIZE);
}


#define FIRMWARE_SIZE 0x8000
#define FIRMWARE_START_ADDRESS 0x8000
#define CHUNK_SIZE 64

int main(int argc, char** argv) {
	uint8_t buf[CHUNK_SIZE];
	uint8_t val = 0;
	int i;

	memset(flash_mem, 0xff, FLASH_SIZE);
	memset(flash_mem_golden, 0xff, FLASH_SIZE);

        flash_start_address = (uint32_t) &flash_mem;

	if(!flash_load_erase(FIRMWARE_START_ADDRESS, FIRMWARE_SIZE)) {
		fprintf(stderr, "flash_load_erase failed\n");
		return 1;
	}

	if(!flash_load_prepare(FIRMWARE_START_ADDRESS, FIRMWARE_SIZE)) {
		fprintf(stderr, "flash_load_prepare failed\n");
		return 1;
	}

	for(i = 0; i < 1 + FIRMWARE_SIZE/CHUNK_SIZE; i++) {
		int portion_size;
		if (CHUNK_SIZE*(i + 1) > FIRMWARE_SIZE)
			portion_size = FIRMWARE_SIZE - (CHUNK_SIZE*i);
		else
			portion_size = CHUNK_SIZE;

		memset(flash_mem_golden+FIRMWARE_START_ADDRESS+(CHUNK_SIZE*i), val, portion_size);
		memset(buf, val++, portion_size);
		if(!flash_load_continue(buf, portion_size)) {
			fprintf(stderr, "flash_load_continue failed\n");
			return 1;
		}
	}
	
	if(!flash_load_finalize()) {
		fprintf(stderr, "flash_load_finalize failed\n");
		return 1;
	}


	for(i = 0; i < FLASH_SIZE; i++) {
		if(flash_mem[i] != flash_mem_golden[i]) {
			fprintf(stderr, "Memory content mismatch at position %d: %02X found while %02X expected\n", i, flash_mem[i], flash_mem_golden[i]);
			return 1;
		}
	}
	printf("Flash test passed\n");
	return 0;
}
