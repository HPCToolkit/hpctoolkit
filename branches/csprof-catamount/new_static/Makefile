#
# Libmonitor Makefile.
#
# Make three versions of the library: static, static with threads, and
# dynamic.
#
# $Id: Makefile,v 1.10 2007/08/03 17:30:50 krentel Exp krentel $
#

# Add -DMONITOR_DEBUG_ON to CFLAGS to compile in debugging messages.
CC=gcc
CFLAGS= -g -O -DMONITOR_DEBUG_ON

M_DYN_OBJS=	monitor_main.o
M_INCLUDES=	monitor.h monitor_decls.h

all: dynamic static

dynamic: libmonitor.so

static: monitor_sys_fcns.o static_main.o linker.sh

monitor_main.o: monitor_main.c $(M_INCLUDES)
	$(CC) -c -o $@ $(CFLAGS) -DMONITOR_DYNAMIC -fPIC $<

static_main.o: monitor_main.c $(M_INCLUDES)
	$(CC) -c -o $@ $(CFLAGS) -DMONITOR_STATIC $<

libmonitor.so: $(M_DYN_OBJS) $(M_INCLUDES)
	$(CC) -o $@ $(CFLAGS) -shared -fPIC $(M_DYN_OBJS) -ldl

monitor_sys_fcns.o:
	./copy_libs.sh

linker.sh:
	./make_linker.sh > $@
	chmod 755 $@

clean:
	rm -f *.o *.so *.a linker.sh hello
