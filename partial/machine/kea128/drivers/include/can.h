#ifndef ATS_DRIVERS_CAN_H
#define ATS_DRIVERS_CAN_H

#include <stdint.h>

struct can_pack {
	uint32_t af;
	uint8_t dlc;
	uint8_t data[8];
};

struct can_filter {
	uint32_t filter;
	uint32_t mask;
	int ext;
};

int can_init(uint32_t baud, struct can_filter acc_filter[2]);
void can_send(const struct can_pack* pack);
void can_flush_send();
int can_recv(struct can_pack* pack);

#endif
