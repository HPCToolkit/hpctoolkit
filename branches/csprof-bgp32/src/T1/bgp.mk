#
# platform specific stuff f bgp
#
CC.compute.base := /bgsys/drivers/ppcfloor/comm/bin/mpixlc -qlanglvl=extc99
CC.head.base    := xlc -qlanglvl=extc99

static.arg      := -qstaticlink

ifdef gcc-family
  CC.compute.base := /bgsys/drivers/ppcfloor/comm/bin/mpicc -std=c99
  CC.head.base    := gcc -std=c99
  static.arg      := -static
endif

CC.base         := $(CC.$(base).base)

CC              := $(CC.base) $(CFLAGS) $(CFLAGS2)
