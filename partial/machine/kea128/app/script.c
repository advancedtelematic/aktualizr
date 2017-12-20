#include "script.h"
#include "systimer.h"
#include "led.h"
#include "headlight.h"
#include "flash_load.h"

#define SCRIPT_MAGIC 0x13a0fe89

#define OP_LEDS  0x00
#define OP_LAMPS 0x01
#define OP_WAIT  0x02
#define OP_LOOP  0x03

static uint32_t* script_addr = (uint32_t*) PROGRAM_FLASH_BEGIN;

void script_execute(void)
{
	uint32_t op;
	static uint32_t ts = 0;
	static uint32_t delay = 0;
	static int in_wait = 0;
	uint8_t leds;
	int i;

	if((uint32_t) script_addr < PROGRAM_FLASH_BEGIN || (uint32_t) script_addr >= PROGRAM_FLASH_END)
		script_addr = (uint32_t*) PROGRAM_FLASH_BEGIN;
	op = *script_addr;

	if((uint32_t) script_addr == PROGRAM_FLASH_BEGIN) {
		if(op != SCRIPT_MAGIC) {
			return;
		}
		else {
			script_addr += 2; // skip magic number and firmware version
			op = *script_addr;
		}
	}

	if (in_wait) {
		if(time_passed(ts) > delay)
			in_wait = 0;
		else
			return;
	}

	switch((op >> 24) & 0xFF) {
		case OP_LEDS:
			leds = op & 0x0F;
			for(i = 0; i < 4; i++) {
				led_set(i, leds & 1);
				leds >>= 1;
			}
			break;

		case OP_LAMPS:
			// no brightness regulation yet
			leds = op & 0x0F;
			for(i = 0; i < 4; i++) {
				headlight_set(i, leds & 1);
				leds >>= 1;
			}
			break;

		case OP_WAIT:
			in_wait = 1;
			ts = time_get();
			delay = op & 0xFFFFFF;
			break;

		case OP_LOOP:
			// will be incremented after switch
			script_addr = (uint32_t*) PROGRAM_FLASH_BEGIN;
			break;
		default:
			break;
	}

	script_addr++;
}

void script_init(void)
{
	headlight_init();
	script_addr = (uint32_t*) PROGRAM_FLASH_BEGIN;
}

