#ifndef ATS_BOOT_ISOTP_DISPATCH_H
#define ATS_BOOT_ISOTP_DISPATCH_H

#include <stdint.h>
#include "isotp/isotp.h"

void isotp_dispatch_init(IsoTpMessageReceivedHandler received_cb, IsoTpMessageSentHandler sent_cb, IsoTpShims s);
void isotp_dispatch(void);
int isotp_dispatch_send(const uint8_t* data, uint16_t size, uint32_t af);

#endif // ATS_BOOT_ISOTP_DISPATCH_H
