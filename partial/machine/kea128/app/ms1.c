#include <string.h>
#include "isotp_dispatch.h"

#include "SKEAZ1284.h" /* include peripheral declarations SKEAZ128M4 */
#include "can.h"
#include "led.h"
#include "systimer.h"
#include "flash.h"
#include "flash_load.h"
#include "uds.h"
#include "script.h"

#ifndef CAN_ID
#    error "CAN_ID should be provided"
#endif

/* This variable is necessary for testing the flash loader*/
const uint32_t flash_start_address = 0x00000000;


uint32_t flash_load_startaddr = 0;
uint32_t flash_load_curaddr = 0;
uint32_t flash_load_size = 0;

int uds_in_download = 0;
int uds_in_programming = 0;

uint32_t session_ts;

static uint32_t make_32(uint8_t hh, uint8_t hl, uint8_t lh, uint8_t ll) {
	return (hh << 24) | (hl << 16) | (lh << 8) | ll;
}

void message_received(const IsoTpMessage* message) {
	uint32_t flash_addr;
	uint32_t flash_size;
	uint8_t addr_len;
	uint8_t size_len;
	uint8_t uds_seq_number;
	uint16_t ta = (message->arbitration_id >> 5) & 0x01F; /* TODO: untangle session layer */
	int res;
	int i;
	/* Don't care about AF here, it should be filtered on CAN level */
	/* Switch over SID */
	switch (message->payload[0]) {
		case 0x10: /* DiagnosticSessionControl */
			if(message->size < 2) {
				send_uds_error(ta, 0x10, 0x13); /* Invalid format */
				break;
			}

			if(message->payload[1] ==  0x01) {
				uds_in_programming = false;
			} else if (message->payload[1] ==  0x02) {
				uds_in_programming = true;
				session_ts = time_get();
			} else {
				send_uds_error(ta, 0x10, 0x12); /* Subfunction not supported */
				break;
			}

			send_uds_positive_sessioncontrol(ta, message->payload[1]);
			break;

		case 0x31: /* RoutineControl*/
			if(!uds_in_programming) {
				send_uds_error(ta, 0x31, 0x22); /* Conditions not correct */
				break;
			}

			session_ts = time_get();

			if(message->size < 4) {
				send_uds_error(ta, 0x31, 0x13); /* Invalid Format */
				break;
			}
			if(message->payload[1] != 0x01) { /* Start routine*/
				/* TODO: support other routine operations */
				send_uds_error(ta, 0x31, 0x12); /* SFNS */
				break;
			}
			if(message->payload[2] != 0xff &&message->payload[3] != 0x00) {
				send_uds_error(ta, 0x31, 0x12); /* SFNS */
				break;
			}
			if(message->size != 12) {
				send_uds_error(ta, 0x31, 0x13); /* Invalid Format */
				break;
			}
			flash_addr = make_32(message->payload[4], message->payload[5], message->payload[6], message->payload[7]);
			flash_size = make_32(message->payload[8], message->payload[9], message->payload[10], message->payload[11]);

			if((flash_addr < PROGRAM_FLASH_BEGIN) ||
					(flash_addr + flash_size) >= PROGRAM_FLASH_END ||
					(flash_addr + flash_size) < flash_addr) { /*overflow*/
				send_uds_error(ta, 0x31, 0x31); /* ROOR */
				break;
			}
			res = flash_load_erase(flash_addr, flash_size);
			if(res)
				send_uds_error(ta, 0x31, 0x10); /* General Error */
			else
				send_uds_positive_routinecontrol(ta, message->payload[1], (message->payload[2] << 8) | message->payload[3]);
			break;
		case 0x11: /* ECUReset */
			if(message->size < 2) {
				send_uds_error(ta, 0x11, 0x13); /* Invalid format */
				break;
			}

			if(message->payload[1] != 0x01) { /* Only hard reset is supported */
				send_uds_error(ta, 0x11, 0x12); /* Subfunction not supported */
				break;
			}
			send_uds_positive_ecureset(ta, message->payload[1]);
			can_flush_send();
			NVIC_SystemReset();
			break;

		case 0x34: /* Request download */
			if(!uds_in_programming) {
				send_uds_error(ta, 0x34, 0x70); /* Not accepted */
				break;
			}

			session_ts = time_get();

			if(message->size < 3) {
				send_uds_error(ta, 0x34, 0x13); /* Invalid Format */
				break;
			}
			if(message->payload[1] != 0x00) { /* Only support raw data */
				send_uds_error(ta, 0x34, 0x31); /* ROOR */
				break;
			}
			addr_len = message->payload[2] & 0x0F;
			size_len = (message->payload[2] >> 4) & 0x0F;
			if(addr_len == 0 || addr_len > 4 || size_len == 0 || size_len > 4)  {
				send_uds_error(ta, 0x34, 0x31); /* ROOR */
				break;
			}

			if(message->size < 3+addr_len+size_len) {
				send_uds_error(ta, 0x34, 0x13); /* Invalid Format */
				break;
			}

			flash_addr = 0;
			for(i = 3; i < addr_len+3; i++) {
				flash_addr <<= 8;
				flash_addr |=message->payload[i];
			}
			flash_size = 0;
			for(;i < addr_len+size_len+3; i++) {
				flash_size <<= 8;
				flash_size |=message->payload[i];
			}

			if((flash_addr < PROGRAM_FLASH_BEGIN) ||
					(flash_addr + flash_size) >= PROGRAM_FLASH_END ||
					(flash_addr + flash_size) < flash_addr) { /*overflow*/
				send_uds_error(ta, 0x34, 0x31); /* ROOR */
				break;
			}

			flash_load_prepare(flash_addr, flash_size);
			uds_seq_number = 0x00;
			uds_in_download = 1;
			flash_load_startaddr = flash_addr;
			flash_load_curaddr = flash_addr;
			flash_load_size = flash_size;
			send_uds_positive_reqdownload(ta, UDS_MAX_BLOCK);
			break;
		case 0x36: /* TransferData */
			session_ts = time_get();

			if(!uds_in_download) {
				send_uds_error(ta, 0x36, 0x24); /* Sequence Error */
				break;
			}
			if(message->size < 2) {
				send_uds_error(ta, 0x36, 0x13); /* Invalid Format */
				break;
			}
			if(message->payload[1] == uds_seq_number) { /* Repeated segment, acknowledge and ignore*/
				send_uds_positive_transferdata(ta, uds_seq_number);
				break;
			} else if(message->payload[1] != (uint8_t) (uds_seq_number+1)) {
				send_uds_error(ta, 0x36, 0x24); /* Sequence Error */
				break;
			}

			if(flash_load_curaddr +message->size - 2 > flash_load_startaddr+flash_load_size) {
				send_uds_error(ta, 0x36, 0x31); /* ROOR */
				break;
			}
			flash_load_continue(message->payload+2,message->size-2);
			uds_seq_number = message->payload[1];
			send_uds_positive_transferdata(ta, uds_seq_number);
			break;

		case 0x37: /* RequestTransferExit */
			session_ts = time_get();

			if(!uds_in_download) {
				send_uds_error(ta, 0x37, 0x24); /* Sequence Error */
				break;
			}
			flash_load_finalize();
			uds_in_download = 0;
			send_uds_positive_transferexit(ta);
			break;

		case 0x22: /* ReadDataByIdentifier */
			if(message->size < 3) {
				send_uds_error(ta, 0x22, 0x13); /* Invalid Format */
				break;
			}

			/* Only support one piece of data per message */
			if(message->size > 3) {
				send_uds_error(ta, 0x22, 0x14); /* Response too long */
				break;
			}

			switch((message->payload[1] << 8) | (message->payload[2])) {
				case HW_ID_DID:
					send_uds_positive_readdata(ta, HW_ID_DID, (const uint8_t* )UPTANE_HARDWARE_ID, strlen(UPTANE_HARDWARE_ID));
					break;
				case ECU_SERIAL_DID:
					send_uds_positive_readdata(ta, ECU_SERIAL_DID, (const uint8_t*) UPTANE_ECU_SERIAL, strlen(UPTANE_ECU_SERIAL));
					break;
				default:
					send_uds_error(ta, 0x22, 0x31); /* ROOR */
					break;
			}
			break;
		default:
			send_uds_error(ta, message->payload[0], 0x11); /* Service not supported */
			break;

	}
}

void main(void) {
  int i;
  uint8_t led_mask = 0x01;
  struct can_pack pack;
  struct can_filter can_filters[2];

  IsoTpShims isotp_shims;
  IsoTpReceiveHandle isotp_receive_handle;
  int isotp_in_progress = 0;

  time_init();
  led_init();
  flash_init();
  
  script_init();

  can_filters[0].filter = 0xffffffe0 | CAN_ID;
  can_filters[0].mask = 0xffffffe0;
  can_filters[0].ext = 0;

  can_filters[1].filter = 0xffffffe0 | CAN_ID;
  can_filters[1].mask = 0xffffffe0;
  can_filters[1].ext = 0;

  can_init(125000, can_filters);

  __enable_irq();

  for(i = 0; i < 4; i++)
	led_set(i, 1);

  time_delay(500);

  for(i = 0; i < 4; i++)
	led_set(i, 0);

  isotp_shims = isotp_init_shims(NULL, send_can_isotp, NULL, NULL);

  isotp_dispatch_init(message_received, NULL, isotp_shims);
  for(;;) {
	if(uds_in_programming) {
		/* TODO: figure out real timeout (S3) */
		if(time_passed(session_ts) > 60000) {
			uds_in_download = 0;
			uds_in_programming = 0;
		}
	}
	
	if(!uds_in_programming) {
		script_execute();
	}

	isotp_dispatch();
  }
}
