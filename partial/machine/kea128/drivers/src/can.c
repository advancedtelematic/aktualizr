#include "can.h"
#include "SKEAZ1284.h"

#define CAN_IN_BUF_SIZE 16
#define CAN_OUT_BUF_SIZE 16

// divide cycle into 20 TQ (1 + 13 + 6)
#define CAN_TSEG1 (13 - 1)
#define CAN_TSEG2 (6 - 1)
#define CAN_SJW (3 - 1)

#define can_out_crit_beg() {NVIC_DisableIRQ(MSCAN_TX_IRQn);}
#define can_out_crit_end() {NVIC_EnableIRQ(MSCAN_TX_IRQn);}
#define can_in_crit_beg() {NVIC_DisableIRQ(MSCAN_RX_IRQn);}
#define can_in_crit_end() {NVIC_EnableIRQ(MSCAN_RX_IRQn);}

#define CAN_ID_ADDR 0x1FFFF
static struct {
	struct can_pack buf[CAN_IN_BUF_SIZE];
	int beg;
	int end;
	int len;
} can_in_buf;

static struct {
	struct can_pack buf[CAN_OUT_BUF_SIZE];
	int beg;
	int end;
	int len;
} can_out_buf;

static void can_send_low(const struct can_pack* pack);

// leaves only least significant set bit. Mostly used for CANTFLG
static inline uint8_t least_set(uint8_t reg)
{
	return reg & (-reg);
}

static volatile int transmitting = 1;

void can_flush_send() {
	while(transmitting);
}

void MSCAN_RX_IRQHandler(void)
{
	struct can_pack* pack;
	int i;

	if(MSCAN->CANRFLG & 1) { //packet received
		if(can_in_buf.len < CAN_IN_BUF_SIZE) {
			pack = &can_in_buf.buf[can_in_buf.beg];
			if(MSCAN->REIDR1 & 0x08) {
				// extended ID
				pack->af = (1UL << 31) |
					   (MSCAN->REIDR3 >> 1) |
					   ((MSCAN->REIDR2) << 7) |
					   ((MSCAN->REIDR1 & 0x07) << 15) |
					   ((MSCAN->REIDR1 >> 5) << 18) |
					   (MSCAN->REIDR0 << 21);
			}
			else {
				// standard ID
				pack->af = MSCAN->RSIDR1 >> 5 | 
					   MSCAN->RSIDR0 << 3;
			}
			pack->dlc = MSCAN->RDLR & 0x0F;
			for(i = 0; i < pack->dlc; i++)
				pack->data[i] = MSCAN->REDSR[i];

			can_in_buf.len++;
			if(++can_in_buf.beg >= CAN_IN_BUF_SIZE)
				can_in_buf.beg = 0;
		}
		//acknowledge;
		MSCAN->CANRFLG |= 1;
	}
}

void MSCAN_TX_IRQHandler(void)
{
	uint8_t bufmask = MSCAN->CANTFLG & MSCAN->CANTIER & 0x07; // three hardware buffers
	int i;

	if(MSCAN->CANRFLG & (1 << 6)) { //status change
		//no action now, just acknowledge;
		MSCAN->CANRFLG |= (1 << 6);
	}

	if(MSCAN->CANRFLG & (1 << 1)) { //overrun
		//no action now, just acknowledge;
		MSCAN->CANRFLG |= (1 << 1);
	}

	for(i = 0; i < 3; i++) {
		if(bufmask & (1 << i)) {
			if(can_out_buf.len) {
				MSCAN->CANTIER &= ~(1 << i);
				can_send_low(&can_out_buf.buf[can_out_buf.beg]);
				// can_send_low() sets CANTFLG acknowledging the interrupt
				can_out_buf.len--;
				if(++can_out_buf.beg >= CAN_OUT_BUF_SIZE)
					can_out_buf.beg = 0;
			} else {
				// nothing to transmit, turn the interrupt off
				MSCAN->CANTIER &= ~(1 << i);
				transmitting = 0;
			}
		}
	}
}

int can_init(uint32_t baud, struct can_filter acc_filter[2])
{
	int brp;
	uint32_t filter;
	uint32_t mask;
	int ext;

	NVIC_DisableIRQ(MSCAN_RX_IRQn);
	NVIC_DisableIRQ(MSCAN_TX_IRQn);

	// 20 TQ and BusClock = SystemCoreClock/2
	brp = SystemCoreClock/(20*2*baud) - 1;
	if(brp < 0 || brp > 63)
		return 0;

	SIM->SCGC |= SIM_SCGC_MSCAN_MASK;

	if(MSCAN->CANCTL1 & (1 << 7)) { // CANE == 1, reinit
		MSCAN->CANCTL0 |= (1 << 1); // request transition into sleep mode
		while(!(MSCAN->CANCTL1 & (1 << 1))); // nothing can go wrong, right?

		MSCAN->CANCTL0 |= 1; // request transition into init mode
		while(!(MSCAN->CANCTL1 & 1)); // initialization is acknowledged
	}
	else {
		MSCAN->CANCTL1 |= (1 << 7) | (1 << 6); // enable CAN, clock from bus clock
		while(!(MSCAN->CANCTL1 & 1)); // initialization is acknowledged
	}

	MSCAN->CANBTR0 = (brp & 0x3F) | (CAN_SJW << 6);
	MSCAN->CANBTR1 = CAN_TSEG1 | (CAN_TSEG2 << 4);

	filter = acc_filter[0].filter;
	mask = acc_filter[0].mask;
	ext = acc_filter[0].ext;
	if(ext) {
		MSCAN->CANIDAR_BANK_1[0] = (filter >> 21) & 0xFF;
		MSCAN->CANIDAR_BANK_1[1] = ((filter >> 15) & 0x07) |
					   (1 << 3) |
					   (1 << 4) |
					   (((filter >> 18) & 0x07) << 5);
		MSCAN->CANIDAR_BANK_1[2] = (filter >> 7) & 0xFF;
		MSCAN->CANIDAR_BANK_1[3] = (filter << 1) & 0xFE; //ignoring rtr

		MSCAN->CANIDMR_BANK_1[0] = (mask >> 21) & 0xFF;
		MSCAN->CANIDMR_BANK_1[1] = (mask >> 15) & 0xEF;
		MSCAN->CANIDMR_BANK_1[2] = (mask >> 7) & 0xFF;
		MSCAN->CANIDMR_BANK_1[3] = ((mask << 1) & 0xFF) | 1; //ignoring rtr
	} else {
		MSCAN->CANIDAR_BANK_1[0] = (filter >> 3) & 0xFF;
		MSCAN->CANIDAR_BANK_1[1] = (filter & 0x07) << 5;

		MSCAN->CANIDMR_BANK_1[0] = (mask >> 3) & 0xFF;
		MSCAN->CANIDMR_BANK_1[1] = (mask & 0x07) << 5 |
					   (1 << 4) | // ignoring rtr
					   0x07; //ignoring reserved
	}

	filter = acc_filter[1].filter;
	mask = acc_filter[1].mask;
	ext = acc_filter[1].ext;

	if(ext) {
		MSCAN->CANIDAR_BANK_2[0] = (filter >> 21) & 0xFF;
		MSCAN->CANIDAR_BANK_2[1] = ((filter >> 15) & 0x07) |
					   (1 << 3) |
					   (1 << 4) |
					   (((filter >> 18) & 0x07) << 5);
		MSCAN->CANIDAR_BANK_2[2] = (filter >> 7) & 0xFF;
		MSCAN->CANIDAR_BANK_2[3] = (filter << 1) & 0xFE; //ignoring rtr

		MSCAN->CANIDMR_BANK_2[0] = (mask >> 21) & 0xFF;
		MSCAN->CANIDMR_BANK_2[1] = (mask >> 15) & 0xEF;
		MSCAN->CANIDMR_BANK_2[2] = (mask >> 7) & 0xFF;
		MSCAN->CANIDMR_BANK_2[3] = ((mask << 1) & 0xFF) | 1; //ignoring rtr
	} else {
		MSCAN->CANIDAR_BANK_2[0] = (filter >> 3) & 0xFF;
		MSCAN->CANIDAR_BANK_2[1] = (filter & 0x07) << 5;

		MSCAN->CANIDMR_BANK_2[0] = (mask >> 3) & 0xFF;
		MSCAN->CANIDMR_BANK_2[1] = (mask & 0x07) << 5 |
					   (1 << 4) | // ignoring rtr
					   0x07; //ignoring reserved
	}

	can_out_buf.beg = can_out_buf.end = can_out_buf.len = 0;
	can_in_buf.beg = can_in_buf.end = can_in_buf.len = 0;

	MSCAN->CANCTL1 &= ~(1 << 4); // exit listen mode
	MSCAN->CANCTL0 &= ~(1); // exit initialization mode
	while(MSCAN->CANCTL1 & 1); // initialization is acknowledged

	MSCAN->CANRIER = 	 1  | // RX interrupt
			 (0x3 << 2) | // All receive status changes
			 (0x3 << 4) | // All tranmit status changes
			 (0x1 << 6);  // Status interrupt

	// switch tranciever to normal mode
	GPIOA_PDDR |= (1 << 24);
	GPIOA_PCOR |= (1 << 24);
	NVIC_EnableIRQ(MSCAN_RX_IRQn);
	NVIC_EnableIRQ(MSCAN_TX_IRQn);
	return 1;
}

static void copy_pack(const struct can_pack* from, struct can_pack* to)
{
	int i;

	to->af = from->af;
	to->dlc = from->dlc;
	for(i = 0; i < 8; i++)
		to->data[i] = from->data[i];
}

int can_recv(struct can_pack* pack)
{
	int idx;
	int i;

	can_in_crit_beg();
	if(can_in_buf.len == 0) {
		can_in_crit_end();
		return 0;
	}

	idx = can_in_buf.end;
	pack->af = can_in_buf.buf[idx].af;
	pack->dlc = can_in_buf.buf[idx].dlc;
	for(i = 0; i < can_in_buf.buf[idx].dlc; i++)
		pack->data[i] = can_in_buf.buf[idx].data[i];

	can_in_buf.len--;
	if(++can_in_buf.end >= CAN_IN_BUF_SIZE)
		can_in_buf.end = 0;

	can_in_crit_end();
	return 1;
}

void can_send(const struct can_pack* pack)
{
	can_out_crit_beg();
	if(can_out_buf.len >= CAN_OUT_BUF_SIZE)
		goto out;

	transmitting = 1;
	if(can_out_buf.len == 0 && (MSCAN->CANTFLG & 0x07)) {
		can_send_low(pack);
		goto out;
	}

	copy_pack(pack, &can_out_buf.buf[can_out_buf.end]);
	can_out_buf.len++;
	if(++can_out_buf.end >= CAN_OUT_BUF_SIZE)
		can_out_buf.end = 0;

out:
	can_out_crit_end();
}


static void can_send_low(const struct can_pack* pack)
{
	uint8_t bufmask = least_set(MSCAN->CANTFLG & 0x07); // three hardware buffers.
	uint32_t af = pack->af;
	int ext = !!(af & 0x80000000);
	int i;

	if(!bufmask) // free buffer wasn't found. It should never happen.
		return;

	if(pack->dlc > 8)
		return;

	MSCAN->CANTBSEL |= bufmask;

	if(ext) {
		MSCAN->TEIDR3 = (af << 1) & 0xFF;
		MSCAN->TEIDR2 = (af >> 7) & 0xFF;
		MSCAN->TEIDR1 = ((af >> 15) & 0x07) |
					     0x18 | // SRR and extended flags
				((af >> 18) & 0x07) << 5;
		MSCAN->TEIDR0 = (af >> 21) & 0xFF;
	} else {
		MSCAN->TSIDR1 = (af & 0x07) << 5;
		MSCAN->TSIDR0 = (af >> 3) & 0xFF;
	}

	MSCAN->TDLR = pack->dlc & 0x0F;
	for(i = 0; i < pack->dlc; i++)
		MSCAN->TEDSR[i] = pack->data[i];

	MSCAN->CANTFLG = bufmask;
	MSCAN->CANTIER |= bufmask;
}

