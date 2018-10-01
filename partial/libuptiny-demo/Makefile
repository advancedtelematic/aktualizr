# name of your application
APPLICATION = libuptiny-demo

# If no BOARD is found in the environment, use this default:
BOARD ?= kea128ledlightrd

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= ../extern/RIOT

CFLAGS += -Wno-error=sign-compare

CFLAGS += -DJSMN_STRICT -DJSMN_PARENT_LINKS

CFLAGS += -DGNRC_PKTBUF_SIZE=1024

CFLAGS += -DCAN_ISOTP_BS=1
CFLAGS += -DCAN_ISOTP_STMIN=10

CFLAGS_OPT = -Os
# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

export INCLUDES += -I../
DIRS += ../libuptiny
DIRS += ../ed25519

# Modules to include:
USEMODULE += xtimer
USEMODULE += tja1042
USEMODULE += can
USEMODULE += conn_can
USEMODULE += can_trx
USEMODULE += can_isotp
USEMODULE += libuptiny
USEMODULE += ed25519-dlbeer

include $(RIOTBASE)/Makefile.include
