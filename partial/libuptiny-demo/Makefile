# name of your application
APPLICATION = libuptiny-demo

# If no BOARD is found in the environment, use this default:
BOARD ?= kea128ledlightrd

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= ../extern/RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP

CFLAGS += -Wno-error=sign-compare

CFLAGS += -DJSMN_STRICT -DJSMN_PARENT_LINKS

CFLAGS += -DGNRC_PKTBUF_SIZE=1024

CFLAGS_OPT = -O0
# Change this to 0 show compiler invocation lines by default:
QUIET ?= 0

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

USEMODULE += auto_init_can

include $(RIOTBASE)/Makefile.include
