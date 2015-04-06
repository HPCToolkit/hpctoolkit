#
# hardwire the backend c compiler for bgq cnk
#
CC = powerpc64-bgq-linux-gcc
AM_CPPFLAGS = -I/bgsys/drivers/V1R2M1/ppc64 -I/bgsys/drivers/V1R2M1/ppc64/spi/include/kernel/cnk
noinst_LTLIBRARIES = libhardware-thread-id.la
libhardware_thread_id_la_SOURCES = hardware-thread-id.c
