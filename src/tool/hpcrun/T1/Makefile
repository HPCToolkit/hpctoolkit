#
# $Id$
#

CC     := gcc -std=c99
CXX    := g++
MPICC  := mpicc

#LINKPRE := hpclink
#static := -static
# static_lib := $(HOME)/Documents/pkgs-src/hpctoolkit/INSTALL/lib/hpctoolkit/libcsprof.a
# static_lib := $(HOME)/hpctoolkit-install/lib/hpctoolkit/libcsprof.a

LINK.cc := $(LINKPRE) $(CC) $(static)

CFLAGS := -g -I..

tmpi: tmpi.c
	$(MPICC) $< -lm -o $@

mpi: mpi.c
	$(MPICC) -o $@ -g -O $<

t1: t1.o $(static_lib)
	$(LINK.cc) -g $< -lm -o $@

fib: fib.o
	$(CC) -g $< -o $@

dlopen: dlopen.c
	$(CC) -o $@ -g -O $< -ldl

dlopen-test: dlopen-test.c
	gcc -o dlopen-test -g dlopen-test.c -ldl

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
