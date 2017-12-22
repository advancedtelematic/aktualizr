#include "isotp_dispatch.h"
#include "can.h"
#include "systimer.h"

/* Simple dispatcher supporting only one sending and one receiving stream,
 * everything else will be dropped */

static IsoTpReceiveHandle receive_handle;
static IsoTpSendHandle send_handle;
static IsoTpMessage send_message;

static IsoTpShims shims;

static IsoTpMessageReceivedHandler received_callback = NULL;
static IsoTpMessageSentHandler sent_callback = NULL;

int receiving = 0;
int sending = 0;

/* Timestamp to limit sending frequency by 100Hz*/
uint32_t sending_ts;

void isotp_dispatch_init(IsoTpMessageReceivedHandler received_cb, IsoTpMessageSentHandler sent_cb, IsoTpShims s) {
	received_callback = received_cb;
	sent_callback = sent_cb;
	shims = s;

	receiving = sending = 0;
}

void isotp_dispatch() {
	struct can_pack pack;

	if(can_recv(&pack)) {
		if((pack.data[0] >> 4) == 0x3) { /* Flow control */
			/* TODO: make isotp-c work with SA and TA, not AF*/

			if(sending && (pack.af == send_handle.receiving_arbitration_id))
				isotp_receive_flowcontrol(&shims, &send_handle, pack.af, pack.data, pack.dlc);
		} else {
			if(!receiving) {
				receive_handle = isotp_receive(&shims, pack.af, received_callback);
				receiving = 1;
			}

			if(pack.af == receive_handle.arbitration_id) {
				IsoTpMessage message = isotp_continue_receive(&shims, &receive_handle, pack.af, pack.data, pack.dlc);
				/* TODO: Fix error handling in isotp-c */
				if(message.completed && receive_handle.completed)
					receiving = 0 ;
			}
		}
	}

	if(sending && (send_handle.to_send != 0) && (time_passed(sending_ts) > 10)) {
		isotp_continue_send(&shims, &send_handle);

		/* No error handling */
		if(send_handle.completed)
			sending = 0;
		else
			sending_ts = time_get();
	}
}

int isotp_dispatch_send(const uint8_t* data, uint16_t size, uint32_t af) {
	if(sending)
		return 0;

	send_message = isotp_new_send_message(af, data, size);
	send_handle = isotp_send(&shims, &send_message, sent_callback);
	if(send_handle.completed)
		return send_handle.success;
	/* else */
	sending = 1;
	sending_ts = time_get();

	return 1;
}
