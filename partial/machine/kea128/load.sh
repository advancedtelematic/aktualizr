#!/bin/bash

sudo openocd -f ./interface/ftdi/olimex-arm-usb-ocd-h.cfg -f interface/ftdi/olimex-arm-jtag-swd.cfg -f target/ke06.cfg&

/home/oytis/NXP/S32DS_ARM_v1.3/Cross_Tools/gcc-arm-none-eabi-4_9/bin/arm-none-eabi-gdb $1 -batch \
	-ex "target remote :3333" \
	-ex "monitor reset halt" \
	-ex "load" \
	-ex "monitor reset"

sleep 2
