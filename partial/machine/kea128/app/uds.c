#include "uds.h"
#include "isotp_dispatch.h"
#include "can.h"
#include "systimer.h"

#include <string.h>


static uint8_t payload[OUR_MAX_ISO_TP_MESSAGE_SIZE];

int send_can_isotp(uint32_t arbitration_id, const uint8_t* data, uint8_t size, void* private_data) {
	(void) private_data;
        struct can_pack pack;

        if(size > 8)
                return 0;
        pack.af = arbitration_id;
        pack.dlc = size;
        memcpy(pack.data, data, size);
        can_send(&pack);
        return 1;
}

int send_uds_error(uint16_t sa, uint8_t sid, uint8_t nrc) {
	payload[0] = 0x7F | 0x40; /* Error SID */
	payload[1] = sid;
	payload[2] = nrc;

	return isotp_dispatch_send(payload, 3, (CAN_ID << 5) | sa);
}

int send_uds_positive_routinecontrol(uint16_t sa, uint8_t op, uint16_t id) {
	payload[0] = 0x31 | 0x40; /* RoutineControl */
	payload[1] = op;
	payload[2] = id >> 8;
	payload[3] = id & 0xFF;

	return isotp_dispatch_send(payload, 4, (CAN_ID << 5) | sa);
}

int send_uds_positive_sessioncontrol(uint16_t sa, uint8_t type) {
	payload[0] = 0x10 | 0x40; /* SessionControl */
	payload[1] = type;

	/* TODO: figure out actual timeouts we can support */
	payload[2] = 0xFF;
	payload[3] = 0xFF;
	payload[4] = 0xFF;
	payload[5] = 0xFF;

	return isotp_dispatch_send(payload, 6, (CAN_ID << 5) | sa);
}


int send_uds_positive_ecureset(uint16_t sa, uint8_t rtype) {
	payload[0] = 0x11 | 0x40; /* EcuReset */
	payload[1] = rtype;
	payload[2] = 0x00; /* Reset immediately*/

	return isotp_dispatch_send(payload, 3, (CAN_ID << 5) | sa);
}

int send_uds_positive_reqdownload(uint16_t sa, uint16_t maxblock) {
	payload[0] = 0x34 | 0x40; /* RequestDownload */
	payload[1] = 0x20; /* 2 bytes for maximum block size */
	payload[2] = maxblock >> 8;
	payload[3] = maxblock & 0xFF;

	return isotp_dispatch_send(payload, 4, (CAN_ID << 5) | sa);
}

int send_uds_positive_transferdata(uint16_t sa, uint8_t seqn) {
	payload[0] = 0x36 | 0x40; /* TransferData */
	payload[1] = seqn;

	return isotp_dispatch_send(payload, 2, (CAN_ID << 5) | sa);
}

int send_uds_positive_transferexit(uint16_t sa) {
	payload[0] = 0x37 | 0x40; /* RequestTransferExit */

	return isotp_dispatch_send(payload, 1, (CAN_ID << 5) | sa);
}

int send_uds_positive_readdata(uint16_t sa, uint16_t did, const uint8_t* data, uint16_t size) {
	struct can_pack cf_pack;
	if(size+3 > OUR_MAX_ISO_TP_MESSAGE_SIZE)
		return 0;

	payload[0] = 0x22 | 0x40; /* ReadDataByIdentifier */
	payload[1] = did >> 8;
	payload[2] = did & 0xFF;
	memcpy(payload+3, data, size);

	/* Only indicates failure if sending first frame has failed */
	return isotp_dispatch_send(payload, size+3, (CAN_ID << 5) | sa);
}

