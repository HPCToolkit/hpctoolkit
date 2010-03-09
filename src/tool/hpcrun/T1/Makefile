#
# $Id$
#

CC     := gcc -std=gnu99
CXX    := g++
MPICC  := mpicc

ifdef STATIC
   LINKPRE := hpclink
   static_lib := $(HOME)/hpctoolkit-install/lib/hpctoolkit/libcsprof.a
   static := -static
endif

LINK.cc := $(LINKPRE) $(CC) $(static)
LINK.static.cc := hpclink $(CC) -static

CFLAGS := -g -I..

.PHONY: sync-ex
sync-ex: sync Sync-norm/sync-fn.so Sync-override/sync-fn.so

sync: sync-main.c Sync-norm/sync-fn.so
	cp Sync-norm/sync-fn.so .
	$(LINK.cc) -o $@ -lm $(^F)
	rm -rf sync-fn.so

.PHONY:custom
custom: custom-metrics hpcrun-custom.so

deepcallpath: deepcall_main.o $(wildcard DeepCallPaths/f*.c)
	$(LINK.cc) $^ -o $@

tmpi: tmpi.c
	$(MPICC) $< -lm -o $@

mpi: mpi.c
	$(MPICC) -o $@ -g -O $<

t1-static: t1.o
	$(LINK.static.cc) -g $< -lm -o $@


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

Sync-norm/sync-fn.so: sync-fn.c
	$(subd-so)

Sync-override/sync-fn.so: sync-fn-override.c
	$(subd-so)

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
            if [ -x $$f -a ! -d $$f ]; then \
               rm $$f; \
            fi; \
         done
	@rm -rf *.o *.E *~ *.so *.nm.c *.nm.so

.PHONY: showclean
showclean: 
	@for f in $(noext); do \
            if [ -x $$f -a ! -d $$f ]; then \
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

%.so: %.c
	$(CC) -o $@ -g -O -shared -fPIC $^

T:
	# f files $(wildcard DeepCallPaths/f*.c)

define subd-so
if [[ ! -d $(@D) ]]; then mkdir $(@D); fi
$(CC) -o $@ -g -O -shared -fPIC $^
endef
