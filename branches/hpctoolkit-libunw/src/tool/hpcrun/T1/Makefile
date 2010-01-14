#
# $Id$
#

CC     := gcc -std=gnu99
CXX    := g++
MPICC  := mpicc

-include config.mk

ifdef STATIC
   LINKPRE = $(install-prefix)/bin/hpclink
   static_lib = $(install-prefix)/lib/hpctoolkit/libcsprof.a
   static = -static
endif

LINK.cc = $(LINKPRE) $(CC) $(static)

# LINK.static.cc := hpclink $(CC) -static

CFLAGS := -g -I..

.PHONY:custom
custom: custom-metrics hpcrun-custom.so

deepcallpath: deepcall_main.o $(wildcard DeepCallPaths/f*.c)
	$(LINK.cc) $^ -o $@

tmpi: tmpi.c
	$(MPICC) $< -lm -o $@

mpi: mpi.c
	$(MPICC) -o $@ -g -O $<

t1-static: LINKPRE = $(install-prefix)/bin/hpclink
t1-static: static  = -static
t1-static: t1.o
	$(LINK.cc) -g $< -lm -o $@


ifdef NO
t1-static.o: CFLAGS += -static

t1-static.o: t1.c
	$(COMPILE.c) $< -o $@

endif

t1: t1.o
	$(LINK.cc) -g $< -lm -o $@

fib: fib.o
	$(CC) -g $< -o $@

dlopen: dlopen.c
	$(CC) -o $@ -g -O $< -ldl

dlopen-test: dlopen-test.c
	gcc -o dlopen-test -g dlopen-test.c -ldl

dlstress: libsum1.so libsum2.so dlstress.c
	gcc -o $@ -g -O dlstress.c -ldl -lpthread

epoch: libsum1.so libsum2.so epoch.c
	gcc -o $@ -g -O epoch.c -ldl

libsum1.so: sum.c
	$(CC) -o $@ -g -O -shared -fPIC $<

libsum2.so: sum.c
	$(CC) -o $@ -g -O -shared -fPIC -DLIBSUM_TWO $<

hpcrun-custom.so: hpcrun-custom.c
	$(CC) -o $@ -g -O -shared -fPIC $<

.PHONY: custom-metrics-ex

custom-metrics-ex: custom-metrics hpcrun-custom.so
custom-metrics: custom-metrics.c
	$(CC) -g $^ -lm -lpthread -ldl -o $@

.PHONY: early
early: libearly.so emain

libearly.so: early.c
	$(CC) -o $@ -O -g -shared -fPIC $< -ldl -lpthread

emain: emain.c
	$(CC) -o $@ -O -g $< -L. -learly

locks: locks.c
	gcc -o $@ -g -O $< -lpthread

noext = $(filter-out $(wildcard *.*) makefile Makefile,$(wildcard *))

.PHONY: clean
clean: 
	@for f in $(noext); do \
            if [ -x $$f -a ! -d $$f ] && ! file $$f | egrep 'shell *script' 2>&1 > /dev/null; then \
               rm $$f; \
            fi; \
         done
	@rm -rf *.o *.E *~ *.so *.nm.c *.nm.so

.PHONY: sclean
sclean: 
	@for f in $(noext); do \
            if [ -x $$f -a ! -d $$f ] && ! file $$f | egrep 'shell *script' 2>&1 > /dev/null; then \
               echo would rm $$f; \
            fi; \
         done
	@echo would rm -rf *.o *.E *~ *.so *.nm.c *.nm.so

%   :  %.c
	$(CC) -g $< -lm -lpthread -o $@

%   : %.f90
	$(FC) -g $< -o $@

%.o : %.f90
	$(FC) -g -c $< -o $@

%.o : %.c
	$(CC) -g -I.. -c $< -o $@

vpath %.c ..

%.E : %.c
	$(CC) -I.. -E $< > $@

T:
	# f files $(wildcard DeepCallPaths/f*.c)
