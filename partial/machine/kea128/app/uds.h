#ifndef ATS_BOOT_UDS_H
#define ATS_BOOT_UDS_H

#include <stdint.h>

#define UDS_MAX_BLOCK 64

#define HW_ID_DID 0x0001
#define ECU_SERIAL_DID 0x0002

int send_can_isotp(uint32_t arbitration_id, const uint8_t* data, uint8_t size, void* private_data);
int send_uds_error(uint16_t sa, uint8_t sid, uint8_t nrc);
int send_uds_positive_routinecontrol(uint16_t sa, uint8_t op, uint16_t id);
int send_uds_positive_sessioncontrol(uint16_t sa, uint8_t session);
int send_uds_positive_ecureset(uint16_t sa, uint8_t rtype);
int send_uds_positive_reqdownload(uint16_t sa, uint16_t maxblock);
int send_uds_positive_transferdata(uint16_t sa, uint8_t seqn);
int send_uds_positive_transferexit(uint16_t sa);
int send_uds_positive_readdata(uint16_t sa, uint16_t did, const uint8_t* data, uint16_t size);

#endif /* ATS_BOOT_UDS_H */
