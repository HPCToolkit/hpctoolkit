# the Makefile that holds everything together

# work around irritating bug in GNU make--targets defined in the include
# files should not be the default target

PLATFORM := x86-64
CC := gcc
#CHGROOT := 1 # undef for std root version

export PLATFORM
export CC
#export CHGROOT
DO_THREADS := 1
export PERF
#PERF := -DCSPROF_PERF=1

CSPROFLIB := libcsprof.so.1
CSPROFLIB_STATIC := libcsprof.a

#DRIVER ?= mallocprof
# MWF wants async timing, so chg default
DRIVER = itimer
#DRIVER = mallocprof

all: $(CSPROFLIB)

# we develop on Tru64, make it the default
# MWF: working on x86-64, make it (and gcc) the default 30jan07
#PLATFORM ?= ia64

include mkinclude.$(PLATFORM)

LIB_CSFILE := libcsfile.a
INC_CSFILE := -I./hpcfile

INC_MON := -I./monitor/src

INCS := -I. $(INC_CSFILE) $(INC_PFM) $(INC_UNWIND) $(INC_THREAD) $(INC_MON)

# FIXME: PAPI?
ifneq "$(DRIVER)" "itimer"
DEFINES += -DCSPROF_SYNCHRONOUS_PROFILING_ONLY=1
endif
ifeq "$(DRIVER)" "mallocprof"
DEFINES += -DCSPROF_MALLOC_PROFILING=1
endif

ifdef DO_THREADS
  CSPROF_THREADS := -DCSPROF_THREADS=1
  LIB_THREAD := -lpthread
endif

# includes might have set up their own special CFLAGS, so we add
CFLAGS += $(DEFINES) $(CSPROF_THREADS) $(CSPROF_TRAMPOLINE) -I./$(ARCH)

# do not move "csprof_first_func.o" or "csprof_last_func.o"; they need
# to be in those places to force the linker to stitch together the
# library in the proper order and for correct functioning of the profiler
OBJS := first_func.o
OBJS += mon_ovr.o
OBJS += csproflib.o metrics.o mem.o cct.o
OBJS += list.o libc.o epoch.o

ifdef DO_THREADS
   OBJS += pthread.o
endif

OBJS += $(ARCH)/backtrace.o $(ARCH)/swizzle.o $(ARCH)/unsafe.o
OBJS += $(ARCH)/tramp.o $(ARCH)/loadmod.o $(ARCH)/libc.o
OBJS += $(ARCH_OBJS)
OBJS += $(DRIVER).o
OBJS += state.o env.o data.o dump_backtraces.o segv_handler.o
OBJS += last_func.o

LIBS = hpcfile/$(LIB_CSFILE) $(LIB_THREAD) $(LIB_UNWIND) $(LIB_PFM)

#############################################################################
# Compiling Rules:
#############################################################################

$(LIB_CSFILE):
	@cd hpcfile; $(MAKE) -I ..

clean:
	@echo "*** Cleaning csprof ***"
	@/bin/rm -f $(CSPROFLIB) $(CSPROFLIB_STATIC) $(OBJS)
	@cd hpcfile; $(MAKE) -I .. clean


TAGS:
	@etags *.[csh] hpcfile/*.[csh] $(ARCH)/*.[csh]
#TAGS:
#	@echo etags $(OBJS:.o=.[sch])

tarball: 
	@tar -cf csprof.tar $(filter-out %.o %.csp %.log t1 t2 t3,$(wildcard *))
	@gzip --best csprof.tar

.PHONY : all libcsfile clean tarball

$(CSPROFLIB): $(LIB_CSFILE) $(OBJS)
	$(CC) $(LINK_FLAGS) -o $(CSPROFLIB) $(OBJS) $(LIBS)
	mv hpcfile/$(LIB_CSFILE) ./$(CSPROFLIB_STATIC)
	$(AR) $(ARFLAGS) $(CSPROFLIB_STATIC) $(OBJS)
	@echo "*** $(CSPROFLIB) is up to date. ***"

%.o: %.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

%.E: %.c
	$(CC) $(INCS) $(CFLAGS) -E $< > $@
