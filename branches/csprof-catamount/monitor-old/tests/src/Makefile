# $Id: Makefile,v 1.21 2007/03/29 17:07:10 mucci Exp $
SRCS = basic.c fork.c exec.c forkexec.c execshell.c assert.c abort.c sigint.c exit.c dlopen.c _exit.c omp.c _Exit.c nonzeroexit.c real_exit.c pthreads.c pthreadsexit.c badpthreads.c badpthreadscancel.c badpthreadsasynccancel.c 
SCRIPTS = testexec testcsh testsh verify
JAVA_TESTS = ThreadLister.class threads10.class
OBJS = $(SRCS:%.c=%.o)
TESTS = $(SRCS:%.c=%) $(JAVA_TESTS) $(SCRIPTS) 
DESTREGEX = $(patsubst $(root-dir)/tests/src/%,$(build-dir)/tests/%,$(wildcard $(root-dir)/tests/src/*.regex))
TARGETS	= runtests libtesttool-0.so libtesttool-1.so $(TESTS) $(DESTREGEX)
CC = gcc
CFLAGS = -g -O -Wall -I$(root-dir)/src

.PHONY: all
all: $(TARGETS) $(DESTREGEX)

$(DESTREGEX): $(build-dir)/tests/%: $(root-dir)/tests/src/%
	cp $< .

$(JAVA_TESTS): %.class: %.java
	-$(JAVAC) $< 
	-mv $(root-dir)/tests/src/*.class .

verify: verify.pl
	cp $< $@

runtests: runtests.sh
	cp $< $@

testexec: testexec.pl
	cp $< $@

testsh: testsh.sh
	cp $< $@

testcsh: testcsh.csh
	cp $< $@

libtesttool-0.so: testtool.c $(root-dir)/src/monitor.h
	$(CC) -DPROCESS_ONLY -D_REENTRANT $(CFLAGS) $(SHRCFLAGS) $(SHRLDFLAGS) $< -o $@

libtesttool-1.so: testtool.c $(root-dir)/src/monitor.h
	$(CC) -D_REENTRANT $(CFLAGS) $(SHRCFLAGS) $(SHRLDFLAGS) $< -o $@

dlopen: dlopen.o
	$(CC) $(CFLAGS) $< -o $@ -ldl

real_exit: real_exit.o
	$(CC) $(CFLAGS) $< -o $@ -L.. -lmonitor -lpthread

pthreads: pthreads.c 
	$(CC_PTHR) $(CFLAGS) $< -o $@

pthreadsexit: pthreads.c 
	$(CC_PTHR) $(CFLAGS) -DPTHREAD_EXIT $< -o $@

badpthreads: badpthreads.c 
	$(CC_PTHR) $(CFLAGS) $< -o $@

badpthreadscancel: badpthreads.c 
	$(CC_PTHR) $(CFLAGS) -DCANCEL $< -o $@

badpthreadsasynccancel: badpthreads.c 
	$(CC_PTHR) $(CFLAGS) -DCANCEL -DCANCEL_ASYNC $< -o $@

omp: omp.c 
	-$(CC_OMP) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f *~ core.* *.o *.a *.class

.PHONY: clobber
clobber:
	rm -f $(TARGETS)
