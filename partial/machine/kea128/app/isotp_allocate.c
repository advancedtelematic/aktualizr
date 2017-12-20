#include <isotp/isotp.h>
#include <isotp/allocate.h>

#define NUM_ISOTP_BUFS 2
uint8_t buffers[NUM_ISOTP_BUFS][OUR_MAX_ISO_TP_MESSAGE_SIZE];

uint8_t buf_occupied;

uint8_t* allocate(size_t size) {
	int i;

	/* If NUM_ISOTP_BUFS gets large enough consider optimization with clz command */
	for(i = 0; i < NUM_ISOTP_BUFS; i++) {
		if(!(buf_occupied & (1 << i))) {
			buf_occupied |= (1 << i);
			return buffers[i];
		}
	}

	return NULL;
}

void free_allocated(uint8_t* data) {
	int i = ((uint32_t) data - (uint32_t)buffers)/(sizeof(uint8_t) * OUR_MAX_ISO_TP_MESSAGE_SIZE);

	buf_occupied &= ~(1 << i);
}
